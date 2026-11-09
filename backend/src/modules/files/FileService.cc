#include "FileService.h"
#include <cctype>
#include <utility>
namespace{
    Json::Value MakePublicDirectoryData(const Json::Value& rows){
        Json::Value entries(Json::arrayValue);
        for(const auto& row:rows){
            Json::Value entry;
            entry[FR_ID]=std::to_string(row[FR_ID].asUInt64());
            entry[FR_NAME]=row[FR_NAME];
            entry[FR_TYPE]=row[FR_TYPE];
            entry[FR_SIZE]=row[FR_SIZE];
            entry[FR_LAST_MODIFY]=row[FR_LAST_MODIFY];
            entries.append(std::move(entry));
        }

        Json::Value data;
        data[FR_STATUS]=true;
        data[FR_DATA]=std::move(entries);
        return data;
    }

    std::string MakeAttachmentDisposition(std::string_view name){
        constexpr char hex[]="0123456789ABCDEF";
        std::string value="attachment; filename=\"download\"; filename*=UTF-8''";
        for(unsigned char byte:name){
            bool keep=isalpha(byte)||isdigit(byte)||byte=='-'||byte=='_'||byte=='.'||byte=='~';
            if(keep){
                value+=static_cast<char>(byte);
            }else{
                value+='%';
                value+=hex[byte>>4];
                value+=hex[byte&0x0F];
            }
        }
        return value;
    }

    class TransactionCommitAwaiter:public CallbackAwaiter<bool>{
    private:
        TransactionPtr& tx;
    public:
        TransactionCommitAwaiter(TransactionPtr& x):tx(x){};
        void await_suspend(std::coroutine_handle<> handle){
            auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
            tx->setCommitCallback([this,loop,handle](bool commit)->void{
                loop->queueInLoop([this,handle,commit]{
                    setValue(commit);
                    handle.resume();
                });
            });
            tx.reset();
        }        
    };
}
namespace file::server{
        Task<HttpResponsePtr> QueryDirectory(uint64_t userId,
                std::optional<uint64_t> directoryId,std::optional<std::string> path){
            if(path==std::nullopt){
                // 检查校验
                Json::Value meta=co_await file::database::GetEntry(userId,*directoryId);
                if(meta[FR_ID]==-1){
                    if(meta[FR_ERROR]==FR_DB_ERROR){
                        co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                    }else{
                        co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
                    }
                }
                // 根据id查询
                Json::Value res=co_await file::database::ListChildren(userId,*directoryId);
                if(!res[FR_STATUS].asBool()){
                    co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                }
                co_return response::Success(MakePublicDirectoryData(res[FR_DATA]));
            }else{
                Json::Value res=co_await file::database::ListChildren(userId,*path);
                if(!res[FR_STATUS].asBool()){
                    if(res[FR_ERROR]==FR_NO_ENTRY){
                        co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,"路径错误");
                    }else{
                        co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                    }
                }
                co_return response::Success(MakePublicDirectoryData(res[FR_DATA]));
            }
        }


        Task<HttpResponsePtr> CreateDirectory(uint64_t userId,uint64_t parentId,const std::string&name){
            // 判断 parentid 跟 userid是否匹配 匹配之后 尝试直接创建 如果MySQL拒绝就返回错误即可
            Json::Value meta=co_await file::database::GetEntry(userId,parentId);
            if(meta[FR_ID]==-1){
                // 分析 fr_error
                // meta[FR_ERROR]
                if(meta[FR_ERROR]==FR_NO_ENTRY_OR_MISMATCH){
                    co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
                }else{
                    co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                }
            }
            // 有id 但是type是文件
            if(meta[FR_TYPE].asString()==FR_FILE){
                co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
            }
            // 是文件夹 并且身份验证通过 尝试写入
            Json::Value metadata;
            metadata[FR_NAME]=name;
            metadata[FR_FATHER]=parentId;
            Json::Value res=co_await file::database::CreateDirectory(userId,metadata);

            
            if(res[FR_ID]==-1){
                // 第一种 数据库异常 第二种 重名冲突
                if(res[FR_ERROR]==FR_DB_ERROR){
                    co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                }else{
                    co_return response::Failure(HttpStatusCode::k409Conflict,errors::NAME_CONFLICT,"重名文件冲突");
                }
            }
            // 正常写入
            Json::Value req;
            req["status"]=true;
            req[FR_ID]=res[FR_ID];
            co_return response::Success(req);
        }
        
        
        Task<HttpResponsePtr> DeleteEntries(uint64_t userId,const std::vector<uint64_t>& entryIds){
            // 先判断用户权限 然后尝试删除
            Json::Value meta=co_await file::database::GetEntries(userId,entryIds);
            if(!meta[FR_STATUS].asBool()){
                //目前来看就一个地方出错 即数据库异常
                co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务异常");
            }
            if(meta[FR_DATA].size()!=entryIds.size()){
                // 说明有目录 异常
                co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
            }
            // 目前一切正常 只有一种可能情况 entryids 包含根目录 但是这个在删除的时候 会顺便排查 暂时不用管
            // 先删除文件 然后再物理删除 删除文件成功就返回给用户成功
            auto tx=co_await app().getDbClient()->newTransactionCoro();
            auto result=co_await file::database::DeleteEntries(tx,userId,entryIds);
            if(!result[FR_STATUS].asBool()){
                // 删除失败了 就回滚
                tx->rollback();
                co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务错误");
            }
            bool success=co_await TransactionCommitAwaiter{tx};
            if(!success){
                // 没成功
                co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务错误");
            }
            // 删除成功
            Json::Value res;
            res["status"]=true;
            // 要进行物理操作
            for(auto file:result[FR_DATA]){
                if(file[FR_TYPE]==FR_FOLDER)continue;
                // 是文件 尝试删除
                bool success=co_await FastDfsStorage::getInstance().deleteFile(file[FR_PATH].asString());
                if(!success){
                    LOG_ERROR<<"delete file error:"<<file[FR_PATH].asString();
                }
            }
            co_return response::Success(res);

        }
        
        
        Task<HttpResponsePtr> DownloadFile(uint64_t userId,uint64_t fileId){
            // 先判断用户权限 以及是否是正常文件
            Json::Value meta=co_await file::database::GetEntry(userId,fileId);
            if(meta[FR_ID]==-1){
                if(meta[FR_ERROR]==FR_DB_ERROR){
                    co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"服务出错");
                }else{
                    co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
                }
            }
            if(meta[FR_TYPE]==FR_FOLDER){
                co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"错误操作");
            }

            // 权限正常 文件正常 根据path字段 转接给nginx
            auto resp=HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->addHeader("X-Accel-Redirect","/_download/"+meta[FR_PATH].asString());
            resp->addHeader("Content-Disposition",MakeAttachmentDisposition(meta[FR_NAME].asString()));
            resp->addHeader("Cache-Control","private, no-store");

            co_return resp;
        }
}
