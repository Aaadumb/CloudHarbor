#include "UploadService.h"
#include "NumericLimits.h"
#include "TransactionCommitAwaiter.h"
#include <charconv>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <unordered_set>
constexpr int chunk_size=5*1024*1024;
#define US_MSG_FAILED "上传失败"
#define US_MSG_ENTRY_CONFLICT "存在同名条目"
#define US_MSG_BAD_TASK_DATA "上传任务数据异常"
#define US_MSG_SAVE_CHUNK_FAILED "保存分片失败"
#define US_MSG_DIR_NOT_FOUND "目标目录不存在"
#define US_ERR_CHUNKS_MISSING "UPLOAD_CHUNKS_MISSING"
#define US_MSG_CHUNKS_MISSING "上传分片尚未收齐"
#define US_MSG_BAD_MODIFIED_TIME "文件修改时间无效"
#define US_MSG_DIR_CHECK_FAILED "暂时无法检查目标目录"
#define US_MSG_NAME_CHECK_FAILED "暂时无法检查文件名"
#define US_MSG_FILE_CONFLICT "重名文件冲突"

namespace{
    std::string FormatLastModified(uint64_t milliseconds){
        if(milliseconds>numeric::MaxFileModifiedMilliseconds)return {};

        const auto seconds=static_cast<std::time_t>(milliseconds/1000);
        std::tm utc{};
        if(!gmtime_r(&seconds,&utc))return {};

        char datetime[24];
        if(std::strftime(datetime,20,"%Y-%m-%d %H:%M:%S",&utc)!=19)return {};
        std::snprintf(datetime+19,5,".%03u",static_cast<unsigned>(milliseconds%1000));
        return datetime;
    }

    // 单进程内只持有集合锁来登记任务，协程挂起期间只保留任务占用记录。
    std::mutex activeUploadsMutex;
    std::unordered_set<std::string> activeUploads;

    class ChunkTaskGuard{
    public:
        explicit ChunkTaskGuard(const std::string& uploadId):uploadId(uploadId){
            std::lock_guard lock(activeUploadsMutex);
            acquired=activeUploads.insert(uploadId).second;
        }
        ChunkTaskGuard(const ChunkTaskGuard&)=delete;
        ChunkTaskGuard& operator=(const ChunkTaskGuard&)=delete;
        ~ChunkTaskGuard(){
            if(acquired){
                std::lock_guard lock(activeUploadsMutex);
                activeUploads.erase(uploadId);
            }
        }
        explicit operator bool() const{return acquired;}

    private:
        const std::string& uploadId;
        bool acquired=false;
    };

    HttpResponsePtr UploadTaskMissing(){
        return response::Failure(k404NotFound,"UPLOAD_NOT_FOUND","上传任务不存在");
    }

    HttpResponsePtr UploadTaskBusy(){
        return response::Failure(k409Conflict,"UPLOAD_BUSY","上传任务正在处理");
    }

    Task<HttpResponsePtr> LoadOwnedTask(uint64_t userId,const std::string& uploadId,Json::Value& task,
                                      bool validateMetadata=true){
        task=co_await upload::chunk::GetTask(uploadId);
        if(!task["status"].asBool()){
            if(task["error"]==UTS_BAD_ID){
                co_return response::ValidationError("上传任务编号无效");
            }
            if(task["error"]==UTS_NO_TASK)co_return UploadTaskMissing();
            co_return response::ServiceUnavailable("暂时无法读取上传任务");
        }

        const auto& metadata=task["metadata"];
        if(metadata["owner_id"].asUInt64()!=userId)co_return UploadTaskMissing();
        // 取消只需验证归属，允许用户清理旧版留下的超限任务。
        if(!validateMetadata)co_return nullptr;
        const auto size=metadata["size"].asUInt64();
        const auto chunkSize=metadata["chunk_size"].asUInt64();
        const auto totalChunks=metadata["total_chunks"].asUInt64();
        if(size>numeric::MaxSafeInteger || chunkSize==0 || chunkSize>numeric::MaxSafeInteger ||
           metadata["last_modified"].asUInt64()>numeric::MaxFileModifiedMilliseconds ||
           totalChunks!=(size==0 ? 1 : 1+(size-1)/chunkSize)){
            LOG_ERROR<<"invalid upload chunk geometry:"<<uploadId;
            co_return response::InternalError(US_MSG_BAD_TASK_DATA);
        }
        for(const auto& index:task["uploaded_chunks"]){
            if(!index.isUInt64() || index.asUInt64()==0 || index.asUInt64()>totalChunks){
                co_return response::InternalError(US_MSG_BAD_TASK_DATA);
            }
        }
        co_return nullptr;
    }

    Task<HttpResponsePtr> CheckUploadDestination(uint64_t userId,uint64_t parentId,const std::string& name){
        const auto parent=co_await file::database::GetEntry(userId,parentId);
        if(parent[FR_ERROR]==FR_DB_ERROR){
            co_return response::ServiceUnavailable(US_MSG_DIR_CHECK_FAILED);
        }
        if(parent[FR_ID]==-1 || parent[FR_TYPE]!=FR_FOLDER){
            co_return response::NotFound(US_MSG_DIR_NOT_FOUND);
        }
        const auto check=co_await file::database::FindChildEntry(userId,parentId,name);
        if(check[FR_ID]!=-1){
            co_return response::NameConflict(US_MSG_ENTRY_CONFLICT);
        }
        if(check[FR_ERROR]!=FR_NO_ENTRY){
            co_return response::ServiceUnavailable(US_MSG_NAME_CHECK_FAILED);
        }
        co_return nullptr;
    }

    Task<void> CleanupCompletedUpload(const std::string& uploadId,uint64_t fileId){
        try{
            const auto result=co_await upload::chunk::RemoveTask(uploadId,std::to_string(fileId));
            if(!result["status"].asBool()){
                LOG_ERROR<<"event=upload_cleanup_failed operation=complete_upload upload_id="<<uploadId
                         <<" file_id="<<fileId<<" mysql_state=committed error="<<result["error"].asString();
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=upload_cleanup_failed operation=complete_upload upload_id="<<uploadId
                     <<" file_id="<<fileId<<" mysql_state=committed error="<<e.what();
        }
    }

    void LogUploadStorageForReview(const std::string& storageId,uint64_t userId,
                                   const std::string& uploadId,const char* reason){
        if(storageId.empty())return;
        LOG_WARN<<"event=upload_storage_needs_review reason="<<reason<<" storage_id="<<storageId
                <<" user_id="<<userId<<" upload_id="<<uploadId;
    }

    Task<HttpResponsePtr> PersistUpload(uint64_t userId,const Json::Value& metadata,
                                        const std::string& path,const std::string& uploadId={}){
        std::string candidate;
        TransactionPtr tx;
        try{
            const auto sha256=co_await upload::HashStagedFile(path);
            if(sha256.empty())co_return response::InternalError(US_MSG_FAILED);

            // A deduplication hit can be retired after the lookup. In that case retry
            // once using a fresh FastDFS object while the staged bytes are available.
            bool uploadContent=!(co_await file::database::HasStorageObject(sha256));
            for(int attempt=0;attempt<2;++attempt){
                if(uploadContent){
                    candidate=co_await FastDfsStorage::getInstance().uploadFile(path);
                    if(candidate.empty())co_return response::InternalError(US_MSG_FAILED);
                }
                tx=co_await app().getDbClient()->newTransactionCoro();
                const auto result=co_await file::database::CreateFile(tx,userId,metadata,sha256,candidate);
                if(result[FR_ID]==-1){
                    tx->rollback();
                    tx.reset();
                    if(result[FR_ERROR]==FR_STORAGE_MISSING && !uploadContent){
                        uploadContent=true;
                        continue;
                    }
                    LogUploadStorageForReview(candidate,userId,uploadId,"entry_write_failed");
                    if(result[FR_ERROR]==errors::NAME_CONFLICT)co_return response::NameConflict(US_MSG_ENTRY_CONFLICT);
                    if(result[FR_ERROR]==FR_NO_ENTRY)co_return response::NotFound(US_MSG_DIR_NOT_FOUND);
                    co_return response::ServiceUnavailable(US_MSG_FAILED);
                }

                const auto fileId=result[FR_ID].asUInt64();
                if(!uploadId.empty()){
                    bool marked=false;
                    try{
                        marked=co_await upload::chunk::MarkTaskFinalizing(uploadId,"entry:"+std::to_string(fileId));
                    }catch(const std::exception& e){
                        LOG_ERROR<<"event=upload_receipt_failed upload_id="<<uploadId<<" error="<<e.what();
                    }
                    if(!marked){
                        tx->rollback();
                        tx.reset();
                        LogUploadStorageForReview(candidate,userId,uploadId,"receipt_write_failed");
                        co_return response::ServiceUnavailable("暂时无法提交上传任务");
                    }
                }
                if(!(co_await TransactionCommitAwaiter{tx})){
                    LOG_ERROR<<"event=upload_mysql_result_uncertain user_id="<<userId<<" file_id="<<fileId;
                    LogUploadStorageForReview(candidate,userId,uploadId,"mysql_commit_uncertain");
                    co_return response::ServiceUnavailable(US_MSG_FAILED);
                }
                if(!candidate.empty() && candidate!=result[FR_PATH].asString()){
                    LogUploadStorageForReview(candidate,userId,uploadId,"duplicate_upload");
                }
                if(!uploadId.empty())co_await CleanupCompletedUpload(uploadId,fileId);
                co_return response::SuccessWithId(result[FR_ID],"上传成功");
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=upload_persist_failed user_id="<<userId<<" error="<<e.what();
        }
        if(tx){
            tx->rollback();
            tx.reset();
        }
        LogUploadStorageForReview(candidate,userId,uploadId,"persist_failed");
        co_return response::ServiceUnavailable(US_MSG_FAILED);
    }
}

namespace upload{
    Task<HttpResponsePtr> UploadFile(uint64_t userId,uint64_t parentId,
                                    uint64_t lastModified,const HttpFile& file){
        if(file.fileLength()>numeric::MaxSafeInteger){
            co_return response::ValidationError("文件大小超出支持范围");
        }
        const auto modifiedAt=FormatLastModified(lastModified);
        if(modifiedAt.empty()){
            co_return response::ValidationError(US_MSG_BAD_MODIFIED_TIME);
        }

        if(auto failure=co_await CheckUploadDestination(userId,parentId,file.getFileName()))co_return failure;

        std::string path;
        try{
            path=co_await ordinary::SaveOrdinaryFile(file);
        }catch(const std::exception& e){
            LOG_ERROR<<"save ordinary upload error:"<<e.what();
        }
        if(path.empty()){
            co_return response::InternalError("暂存文件失败");
        }

        Json::Value metadata;
        metadata[FR_NAME]=file.getFileName();
        metadata[FR_FATHER]=Json::UInt64(parentId);
        metadata[FR_SIZE]=Json::UInt64(file.fileLength());
        metadata[FR_LAST_MODIFY]=modifiedAt;

        auto result=co_await PersistUpload(userId,metadata,path);
        try{
            if(!(co_await ordinary::RemoveOrdinaryFile(path))){
                LOG_ERROR<<"event=temp_cleanup_failed operation=ordinary_upload user_id="<<userId<<" path="<<path;
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=temp_cleanup_failed operation=ordinary_upload user_id="<<userId
                     <<" path="<<path<<" error="<<e.what();
        }
        co_return result;
    }


    Task<HttpResponsePtr> InitChunkUpload(uint64_t userId,uint64_t parentId,
                                    const std::string& name,uint64_t size,uint64_t lastModified){
        if(size>numeric::MaxSafeInteger){
            co_return response::ValidationError("文件大小超出支持范围");
        }
        // 总共有两次检查 一次是初始化的时候 检查是否有同名文件 一个是合并文件并完成最终上传的时候 再次检查
        // 初次检查没有同名目录 会继续尝试生成uploadid并尝试creattask 后者会把创建目录的工作一并做了
        if(FormatLastModified(lastModified).empty()){
            co_return response::ValidationError(US_MSG_BAD_MODIFIED_TIME);
        }
        if(auto failure=co_await CheckUploadDestination(userId,parentId,name))co_return failure;
        Json::Value meta;
        meta["owner_id"]=userId;
        meta["parent_id"]=parentId;
        meta["name"]=name;
        meta["size"]=size;
        meta["last_modified"]=lastModified;
        // 前端按初始化响应中的 chunkSize 切片。
        meta["chunk_size"]=chunk_size;
        meta["total_chunks"]=size==0?1:1+(size-1)/chunk_size;// 向上取整
        constexpr int maxAttempts=3;
        for(int attempt=0;attempt<maxAttempts;++attempt){
            const auto uploadId=chunk::CreateUploadId();
            if(uploadId.empty())break;

            const auto res=co_await chunk::CreateTask(uploadId,meta);
            if(res["status"].asBool()){
                Json::Value data;
                data["uploadId"]=uploadId;
                data["chunkSize"]=chunk_size;
                data["totalChunks"]=meta["total_chunks"];
                co_return response::Success(data,k201Created);
            }
            if(res["error"]!=UTS_ID_CONFLICT){
                LOG_ERROR<<"create chunk upload task failed:"<<uploadId<<" "<<res["error"].asString();
                break;
            }
        }
        co_return response::ServiceUnavailable("初始化任务失败");
    }


    Task<HttpResponsePtr> UploadFileByChunk(uint64_t userId,const std::string& uploadId,
                                    uint64_t index,const HttpFile&chunk){
        const ChunkTaskGuard guard(uploadId);
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task))co_return failure;
        const auto& metadata=task["metadata"];
        if(!guard || metadata.isMember("cleanup") || metadata.isMember(UTS_FINALIZING_ID)){
            co_return UploadTaskBusy();
        }

        const auto totalChunks=metadata["total_chunks"].asUInt64();
        if(index==0 || index>totalChunks){
            co_return response::ValidationError("分片编号超出范围");
        }
        const auto chunkSize=metadata["chunk_size"].asUInt64();
        const auto size=metadata["size"].asUInt64();
        const auto expectedSize=index==totalChunks ? size-(index-1)*chunkSize : chunkSize;
        if(chunk.fileLength()!=expectedSize){
            co_return response::ValidationError("分片长度不正确");
        }

        using chunk::SaveChunkResult;
        switch(co_await chunk::SaveChunk(uploadId,index,chunk)){
            case SaveChunkResult::Saved:{
                Json::Value data;
                data["status"]=true;
                data["uploadId"]=uploadId;
                data["index"]=Json::UInt64(index);
                data["stored"]=true;
                co_return response::Success(data,k200OK,"保存成功");
            }
            case SaveChunkResult::MissingTask:
                co_return UploadTaskMissing();
            case SaveChunkResult::RedisError:
                co_return response::ServiceUnavailable("暂时无法记录分片");
            case SaveChunkResult::StorageError:
                co_return response::InternalError(US_MSG_SAVE_CHUNK_FAILED);
        }
        co_return response::InternalError(US_MSG_SAVE_CHUNK_FAILED);
    }

    Task<HttpResponsePtr> QueryUploadStatus(uint64_t userId,const std::string& uploadId){
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task))co_return failure;
        const auto& metadata=task["metadata"];
        if(metadata.isMember("cleanup") || metadata.isMember(UTS_FINALIZING_ID))co_return UploadTaskBusy();

        Json::Value data;
        data["uploadId"]=uploadId;
        data["parentId"]=std::to_string(metadata["parent_id"].asUInt64());
        data["name"]=metadata["name"];
        data["size"]=metadata["size"];
        data["lastModified"]=metadata["last_modified"];
        data["chunkSize"]=metadata["chunk_size"];
        data["totalChunks"]=metadata["total_chunks"];
        data["uploadedChunks"]=task["uploaded_chunks"];
        co_return response::Success(data);
    }

    Task<HttpResponsePtr> AskMergeUpload(uint64_t userId,const std::string&uploadId){
        const ChunkTaskGuard guard(uploadId);
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task))co_return failure;
        if(!guard)co_return UploadTaskBusy();
        const auto& metadata=task["metadata"];
        if(metadata.isMember("cleanup")){
            const auto receipt=metadata["cleanup"].asString();
            if(receipt=="cancel")co_return UploadTaskBusy();
            uint64_t fileId=0;
            const auto end=receipt.data()+receipt.size();
            const auto [ptr,ec]=std::from_chars(receipt.data(),end,fileId);
            if(ec!=std::errc{} || ptr!=end || fileId==0){
                co_return response::InternalError(US_MSG_BAD_TASK_DATA);
            }
            co_await CleanupCompletedUpload(uploadId,fileId);
            co_return response::SuccessWithId(Json::UInt64(fileId));
        }
        if(metadata.isMember(UTS_FINALIZING_ID)){
            const auto receipt=metadata[UTS_FINALIZING_ID].asString();
            Json::Value entry;
            if(receipt.starts_with("entry:")){
                const auto* end=receipt.data()+receipt.size();
                uint64_t fileId=0;
                const auto [ptr,ec]=std::from_chars(receipt.data()+6,end,fileId);
                if(ec!=std::errc{} || ptr!=end || fileId==0){
                    co_return response::InternalError(US_MSG_BAD_TASK_DATA);
                }
                entry=co_await database::GetEntry(userId,fileId);
                // A commit with no visible row can be retried after clearing the receipt.
                if(entry[FR_ID]==-1 && entry[FR_ERROR]==FR_NO_ENTRY_OR_MISMATCH){
                    if(!(co_await chunk::ClearTaskFinalizing(uploadId,receipt))){
                        co_return response::ServiceUnavailable("上传提交结果待确认");
                    }
                }else if(entry[FR_ID]==-1){
                    co_return response::ServiceUnavailable("上传提交结果待确认");
                }
            }else{
                entry=co_await database::FindFileByStorageId(userId,metadata["parent_id"].asUInt64(),
                                                            metadata["name"].asString(),receipt);
                if(entry[FR_ID]==-1)co_return response::ServiceUnavailable("上传提交结果待确认");
            }
            if(entry[FR_ID]!=-1){
                co_await CleanupCompletedUpload(uploadId,entry[FR_ID].asUInt64());
                co_return response::SuccessWithId(entry[FR_ID]);
            }
        }

        const auto totalChunks=metadata["total_chunks"].asUInt64();
        const auto& received=task["uploaded_chunks"];
        if(received.size()!=totalChunks){
            co_return response::Failure(k409Conflict,US_ERR_CHUNKS_MISSING,US_MSG_CHUNKS_MISSING);
        }
        uint64_t expectedIndex=1;
        for(const auto& index:received){
            if(index.asUInt64()!=expectedIndex++){
                co_return response::Failure(k409Conflict,US_ERR_CHUNKS_MISSING,US_MSG_CHUNKS_MISSING);
            }
        }

        const auto parentId=metadata["parent_id"].asUInt64();
        const auto name=metadata["name"].asString();
        if(auto failure=co_await CheckUploadDestination(userId,parentId,name))co_return failure;
        const auto modifiedAt=FormatLastModified(metadata["last_modified"].asUInt64());
        if(modifiedAt.empty()){
            co_return response::InternalError(US_MSG_BAD_TASK_DATA);
        }
        const auto size=metadata["size"].asUInt64();
        const auto path=co_await chunk::MergeChunks(uploadId,totalChunks,size);
        if(path.empty()){
            co_return response::InternalError("合并分片失败");
        }
        Json::Value filemeta;
        filemeta[FR_NAME]=name;
        filemeta[FR_SIZE]=Json::UInt64(size);
        filemeta[FR_LAST_MODIFY]=modifiedAt;
        filemeta[FR_FATHER]=Json::UInt64(parentId);
        co_return co_await PersistUpload(userId,filemeta,path,uploadId);
    }

    Task<HttpResponsePtr> AskCancelUpload(uint64_t userId,std::string uploadId){
        const ChunkTaskGuard guard(uploadId);
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task,false))co_return failure;
        const auto& metadata=task["metadata"];
        if(!guard)co_return UploadTaskBusy();
        if(metadata.isMember("cleanup") && metadata["cleanup"]!="cancel"){
            co_return response::Failure(k409Conflict,"UPLOAD_COMPLETED","上传任务已完成");
        }
        if(metadata.isMember(UTS_FINALIZING_ID))co_return UploadTaskBusy();
        const auto res=co_await chunk::RemoveTask(uploadId);
        if(!res["status"].asBool()){
            LOG_ERROR<<"event=upload_cleanup_failed operation=cancel_upload user_id="<<userId
                     <<" upload_id="<<uploadId<<" error="<<res["error"].asString();
            if(res["error"]==UTS_REDIS_ERR){
                co_return response::ServiceUnavailable("暂时无法清理上传任务");
            }
            co_return response::InternalError("清理失败");
        }
        co_return response::Success();
    }
}
