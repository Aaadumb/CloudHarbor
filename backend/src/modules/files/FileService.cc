#include "FileService.h"
#include "AppError.h"
#include "NumericLimits.h"
#include "TransactionCommitAwaiter.h"
#include <atomic>
#include <bit>
#include <cctype>
#include <exception>
#include <string_view>
#include <utility>
#define FS_MSG_SERVICE_ERROR "服务错误"

namespace{
    bool IsValidDirectoryName(std::string_view name){
        if(name.empty() || name=="." || name==".."){
            return false;
        }

        // file.name 是 utf8mb4 VARCHAR(255)，按 Unicode 码点计数。
        constexpr std::size_t maxCharacters=255;
        constexpr char32_t minCodePointByWidth[]={0,0,0x80,0x800,0x10000};
        for(std::size_t offset=0,characters=0;offset<name.size();){
            if(++characters>maxCharacters){
                return false;
            }
            const auto first=static_cast<unsigned char>(name[offset]);
            const int leadingOnes=std::countl_one(first);
            if(leadingOnes==1 || leadingOnes>4){
                return false;
            }
            const auto width=static_cast<std::size_t>(leadingOnes==0 ? 1 : leadingOnes);
            if(width>name.size()-offset){
                return false;
            }

            // 首字节去掉 UTF-8 前缀，每个续字节贡献六位。
            char32_t codePoint=first & (0x7F >> leadingOnes);
            for(std::size_t i=1;i<width;++i){
                const auto byte=static_cast<unsigned char>(name[offset+i]);
                if((byte & 0xC0)!=0x80){
                    return false;
                }
                codePoint=(codePoint << 6) | (byte & 0x3F);
            }
            if(codePoint<minCodePointByWidth[width] || codePoint>0x10FFFF ||
               (codePoint>=0xD800 && codePoint<=0xDFFF)){
                return false;
            }
            if(codePoint=='/' || codePoint<0x20 || (codePoint>=0x7F && codePoint<=0x9F)){
                return false;
            }
            offset+=width;
        }
        return true;
    }

    HttpResponsePtr MakePublicDirectoryResponse(const Json::Value& rows){
        Json::Value entries(Json::arrayValue);
        for(const auto& row:rows){
            if(row[FR_TYPE]==FR_FILE && (!row[FR_SIZE].isUInt64() ||
                    row[FR_SIZE].asUInt64()>numeric::MaxSafeInteger)){
                LOG_ERROR<<"file size outside supported range: file_id="<<row[FR_ID].asString();
                return response::InternalError(FS_MSG_SERVICE_ERROR);
            }
            Json::Value entry;
            entry[FR_ID]=std::to_string(row[FR_ID].asUInt64());
            entry[FR_NAME]=row[FR_NAME];
            entry[FR_TYPE]=row[FR_TYPE];
            entry[FR_SIZE]=row[FR_SIZE];
            entry["lastModified"]=row[FR_LAST_MODIFY];
            entries.append(std::move(entry));
        }

        Json::Value data;
        data[FR_STATUS]=true;
        data[FR_DATA]=std::move(entries);
        return response::Success(std::move(data));
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

    Task<void> DeleteStoredContent(const std::string& storageId,std::string_view operation){
        LOG_INFO<<"event=fastdfs_cleanup_started operation="<<operation<<" storage_id="<<storageId;
        try{
            if(!(co_await FastDfsStorage::getInstance().deleteFile(storageId))){
                LOG_ERROR<<"event=fastdfs_delete_failed operation="<<operation
                         <<" mysql_state=committed storage_id="<<storageId;
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=fastdfs_delete_failed operation="<<operation
                     <<" mysql_state=committed storage_id="<<storageId<<" error="<<e.what();
        }
    }

}
namespace file::server{
        Task<HttpResponsePtr> QueryDirectory(uint64_t userId,
                std::optional<uint64_t> directoryId,std::optional<std::string> path){
            if(path==std::nullopt){
                // 检查校验
                Json::Value meta=co_await file::database::GetEntry(userId,*directoryId);
                if(meta[FR_ID]==-1){
                    if(meta[FR_ERROR]==FR_DB_ERROR){
                        co_return response::ServiceUnavailable();
                    }else{
                        co_return response::Forbidden();
                    }
                }
                if(meta[FR_TYPE]!=FR_FOLDER){
                    co_return response::ValidationError("目录无效");
                }
                // 根据id查询
                Json::Value res=co_await file::database::ListChildren(userId,*directoryId);
                if(!res[FR_STATUS].asBool()){
                    co_return response::ServiceUnavailable();
                }
                co_return MakePublicDirectoryResponse(res[FR_DATA]);
            }else{
                Json::Value res=co_await file::database::ListChildren(userId,*path);
                if(!res[FR_STATUS].asBool()){
                    if(res[FR_ERROR]==FR_NO_ENTRY){
                        co_return response::ValidationError("路径错误");
                    }else{
                        co_return response::ServiceUnavailable();
                    }
                }
                co_return MakePublicDirectoryResponse(res[FR_DATA]);
            }
        }


        Task<HttpResponsePtr> CreateDirectory(uint64_t userId,uint64_t parentId,const std::string&name){
            if(!IsValidDirectoryName(name)){
                co_return response::ValidationError("目录名称无效");
            }
            // 判断 parentid 跟 userid是否匹配 匹配之后 尝试直接创建 如果MySQL拒绝就返回错误即可
            Json::Value meta=co_await file::database::GetEntry(userId,parentId);
            if(meta[FR_ID]==-1){
                // 分析 fr_error
                // meta[FR_ERROR]
                if(meta[FR_ERROR]==FR_NO_ENTRY_OR_MISMATCH){
                    co_return response::Forbidden();
                }else{
                    co_return response::ServiceUnavailable();
                }
            }
            // 有id 但是type是文件
            if(meta[FR_TYPE].asString()==FR_FILE){
                co_return response::Forbidden();
            }
            // 是文件夹 并且身份验证通过 尝试写入
            Json::Value metadata;
            metadata[FR_NAME]=name;
            metadata[FR_FATHER]=parentId;
            Json::Value res=co_await file::database::CreateDirectory(userId,metadata);

            
            if(res[FR_ID]==-1){
                // 第一种 数据库异常 第二种 重名冲突
                if(res[FR_ERROR]==FR_DB_ERROR){
                    co_return response::ServiceUnavailable();
                }else{
                    co_return response::NameConflict("重名文件冲突");
                }
            }
            // 正常写入
            Json::Value req;
            req[FR_STATUS]=true;
            req[FR_ID]=std::to_string(res[FR_ID].asUInt64());
            co_return response::Success(req,HttpStatusCode::k201Created);
        }
        
        
        Task<HttpResponsePtr> DeleteEntries(uint64_t userId,const std::vector<uint64_t>& entryIds){
            // 先判断用户权限 然后尝试删除
            Json::Value meta=co_await file::database::GetEntries(userId,entryIds);
            if(!meta[FR_STATUS].asBool()){
                //目前来看就一个地方出错 即数据库异常
                co_return response::ServiceUnavailable("服务异常");
            }
            if(meta[FR_DATA].size()!=entryIds.size()){
                // 说明有目录 异常
                co_return response::Forbidden();
            }
            // 目前一切正常 只有一种可能情况 entryids 包含根目录 但是这个在删除的时候 会顺便排查 暂时不用管
            // 先删除文件 然后再物理删除 删除文件成功就返回给用户成功
            auto tx=co_await app().getDbClient()->newTransactionCoro();
            Json::Value result;
            try{
                result=co_await file::database::DeleteEntries(tx,userId,entryIds);
            }catch(const std::exception& e){
                tx->rollback();
                LOG_ERROR<<"event=delete_failed user_id="<<userId<<" error="<<e.what();
                co_return response::ServiceUnavailable(FS_MSG_SERVICE_ERROR);
            }
            if(!result[FR_STATUS].asBool()){
                // 删除失败了 就回滚
                tx->rollback();
                co_return response::ServiceUnavailable(FS_MSG_SERVICE_ERROR);
            }
            bool success=co_await TransactionCommitAwaiter{tx};
            if(!success){
                LOG_ERROR<<"event=delete_mysql_result_uncertain user_id="<<userId;
                for(const auto& path:result[FR_DATA]){
                    LOG_ERROR<<"event=delete_mysql_result_uncertain user_id="<<userId
                             <<" storage_id="<<path.asString();
                }
                co_return response::ServiceUnavailable(FS_MSG_SERVICE_ERROR);
            }
            for(const auto& path:result[FR_DATA]){
                co_await DeleteStoredContent(path.asString(),"delete_entry");
            }
            co_return response::Success();

        }
        
        
        Task<HttpResponsePtr> DownloadFile(uint64_t userId,uint64_t fileId){
            // 先判断用户权限 以及是否是正常文件
            Json::Value meta=co_await file::database::GetEntry(userId,fileId);
            if(meta[FR_ID]==-1){
                if(meta[FR_ERROR]==FR_DB_ERROR){
                    co_return response::ServiceUnavailable();
                }else{
                    co_return response::Forbidden();
                }
            }
            if(meta[FR_TYPE]==FR_FOLDER){
                co_return response::Forbidden();
            }

            // 权限正常 文件正常 根据path字段 转接给nginx
            auto resp=HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            response::SetStorageRedirect(resp,meta[FR_PATH].asString());
            resp->addHeader("Content-Disposition",MakeAttachmentDisposition(meta[FR_NAME].asString()));

            co_return resp;
        }
        Task<HttpResponsePtr> getDownloadFileUrl(uint64_t userId,uint64_t id){
            Json::Value meta=co_await file::database::GetEntry(userId,id);
            if(meta[FR_ID]==-1){
                if(meta[FR_ERROR]==FR_DB_ERROR){
                    co_return response::ServiceUnavailable();
                }else{
                    co_return response::Forbidden();
                }
            }
            if(meta[FR_TYPE]==FR_FOLDER){
                co_return response::Forbidden();
            }
            // 文件 权限都正常 返回下载url
            Json::Value res;
            res["url"]="/files/download?id="+std::to_string(id);
            co_return response::Success(res);
        }
    Task<HttpResponsePtr> MoveEntries(uint64_t userId,const std::vector<uint64_t>& entryIds,uint64_t targetId){
        auto tx=co_await app().getDbClient()->newTransactionCoro();
        Json::Value res=co_await file::database::MoveEntries(tx,userId,entryIds,targetId);
        if(!res[FR_STATUS].asBool()){
            // 一种冲突了 一种MySQL异常
            tx->rollback();
            if(res[FR_ERROR]==FR_DB_ERROR)co_return response::ServiceUnavailable();
            else if(res[FR_ERROR]==FR_NO_ENTRY)co_return response::NotFound("目标目录不合法");
            else if(res[FR_ERROR]==FR_EXISTS_CONFLICT)co_return response::NameConflict("目标目录不合法");
            co_return response::NameConflict("重名冲突");
        }
        // 成功 尝试commit
        bool success=co_await TransactionCommitAwaiter{tx};
        if(!success){
            LOG_ERROR<<"提交事务失败";
            co_return response::ServiceUnavailable();
        }
        // 提交成功
        co_return response::Success();
    }
    Task<HttpResponsePtr> CopyEntries(uint64_t userId,const std::vector<uint64_t>& entryIds,uint64_t targetId){
        auto tx=co_await app().getDbClient()->newTransactionCoro();
        Json::Value result;
        try{
            result=co_await database::CopyEntries(tx,userId,entryIds,targetId);
        }catch(const std::exception& e){
            tx->rollback();
            LOG_ERROR<<"event=copy_failed user_id="<<userId<<" error="<<e.what();
            co_return response::ServiceUnavailable();
        }
        if(!result[FR_STATUS].asBool()){
            tx->rollback();
            if(result[FR_ERROR]==FR_NO_ENTRY)co_return response::NotFound("源条目或目标目录不存在");
            if(result[FR_ERROR]==FR_ENTRY_FORBIDDEN)co_return response::Forbidden("根目录不能复制");
            if(result[FR_ERROR]==errors::NAME_CONFLICT)co_return response::NameConflict("目标目录存在同名条目");
            co_return response::ServiceUnavailable();
        }
        if(!(co_await TransactionCommitAwaiter{tx})){
            LOG_ERROR<<"event=copy_mysql_result_uncertain user_id="<<userId<<" target_id="<<targetId;
            co_return response::ServiceUnavailable();
        }
        co_return response::Success();
    }

    Task<void> CleanupStorageObjects(){
        static std::atomic_flag running;
        if(running.test_and_set())co_return;
        struct ResetOnExit{
            std::atomic_flag& lock;
            ~ResetOnExit(){lock.clear();}
        } reset{running};

        static uint64_t afterId=0;
        try{
            const auto objects=co_await database::PendingStorageObjects(afterId);
            if(objects.empty())afterId=0;
            for(const auto& row:objects){
                const auto id=row[FR_ID].as<uint64_t>();
                const auto storageId=row["storage_id"].as<std::string>();
                afterId=id;
                try{
                    if(co_await database::RemovePendingStorageObject(id)){
                        co_await DeleteStoredContent(storageId,"storage_cleanup");
                    }
                }catch(const std::exception& e){
                    LOG_ERROR<<"event=storage_record_delete_failed storage_id="<<storageId<<" error="<<e.what();
                }
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=storage_cleanup_query_failed error="<<e.what();
        }
    }

}
