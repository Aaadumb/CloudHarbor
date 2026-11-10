#include "UploadService.h"
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
#define US_MSG_CHUNKS_MISSING "上传分片尚未收齐"
#define US_MSG_BAD_MODIFIED_TIME "文件修改时间无效"
#define US_MSG_DIR_CHECK_FAILED "暂时无法检查目标目录"
#define US_MSG_NAME_CHECK_FAILED "暂时无法检查文件名"
#define US_MSG_FILE_CONFLICT "重名文件冲突"

namespace{
    std::string FormatLastModified(uint64_t milliseconds){
        constexpr uint64_t maxDatetimeMilliseconds=253402300799999ULL;
        if(milliseconds>maxDatetimeMilliseconds)return {};

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

    Task<HttpResponsePtr> LoadOwnedTask(uint64_t userId,const std::string& uploadId,Json::Value& task){
        task=co_await upload::chunk::GetTask(uploadId);
        if(!task["status"].asBool()){
            if(task["error"]==UTS_BAD_ID){
                co_return response::Failure(k400BadRequest,errors::VALIDATION_ERROR,"上传任务编号无效");
            }
            if(task["error"]==UTS_NO_TASK)co_return UploadTaskMissing();
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"暂时无法读取上传任务");
        }

        const auto& metadata=task["metadata"];
        if(metadata["owner_id"].asUInt64()!=userId)co_return UploadTaskMissing();
        const auto size=metadata["size"].asUInt64();
        const auto chunkSize=metadata["chunk_size"].asUInt64();
        if(chunkSize==0 || metadata["total_chunks"].asUInt64()!=(size==0 ? 1 : 1+(size-1)/chunkSize)){
            LOG_ERROR<<"invalid upload chunk geometry:"<<uploadId;
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_BAD_TASK_DATA);
        }
        co_return nullptr;
    }

    Task<HttpResponsePtr> CheckChunkDestination(uint64_t userId,uint64_t parentId,const std::string& name){
        const auto parent=co_await file::database::GetEntry(userId,parentId);
        if(parent[FR_ERROR]==FR_DB_ERROR){
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_DIR_CHECK_FAILED);
        }
        if(parent[FR_ID]==-1 || parent[FR_TYPE]!=FR_FOLDER){
            co_return response::Failure(k404NotFound,errors::NOT_FOUND,US_MSG_DIR_NOT_FOUND);
        }
        const auto check=co_await file::database::FindChildEntry(userId,parentId,name);
        if(check[FR_ID]!=-1){
            co_return response::Failure(k409Conflict,errors::NAME_CONFLICT,US_MSG_ENTRY_CONFLICT);
        }
        if(check[FR_ERROR]!=FR_NO_ENTRY){
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_NAME_CHECK_FAILED);
        }
        co_return nullptr;
    }

    Task<void> CleanupCompletedUpload(const std::string& uploadId,uint64_t fileId){
        try{
            const auto result=co_await upload::chunk::RemoveTask(uploadId,std::to_string(fileId));
            if(!result["status"].asBool()){
                LOG_ERROR<<"clean completed upload failed:"<<uploadId<<" "<<result["error"].asString();
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"clean completed upload error:"<<uploadId<<" "<<e.what();
        }
    }
}

namespace upload{
    Task<HttpResponsePtr> UploadFile(uint64_t userId,uint64_t parentId,
                                    uint64_t lastModified,const HttpFile& file){
        const auto modifiedAt=FormatLastModified(lastModified);
        if(modifiedAt.empty()){
            co_return response::Failure(k400BadRequest,errors::VALIDATION_ERROR,US_MSG_BAD_MODIFIED_TIME);
        }

        const auto parent=co_await database::GetEntry(userId,parentId);
        if(parent[FR_ERROR]==FR_DB_ERROR){
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_DIR_CHECK_FAILED);
        }
        if(parent[FR_ID]==-1 || parent[FR_TYPE]!=FR_FOLDER){
            co_return response::Failure(k404NotFound,errors::NOT_FOUND,US_MSG_DIR_NOT_FOUND);
        }

        const auto check=co_await database::FindChildEntry(userId,parentId,file.getFileName());
        if(check[FR_ID]!=-1){
            co_return response::Failure(k409Conflict,errors::NAME_CONFLICT,US_MSG_ENTRY_CONFLICT);
        }
        if(check[FR_ERROR]!=FR_NO_ENTRY){
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_NAME_CHECK_FAILED);
        }

        std::string path;
        try{
            path=co_await ordinary::SaveOrdinaryFile(file);
        }catch(const std::exception& e){
            LOG_ERROR<<"save ordinary upload error:"<<e.what();
        }
        if(path.empty()){
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,"暂存文件失败");
        }

        std::string storageId;
        try{
            storageId=co_await FastDfsStorage::getInstance().uploadFile(path);
        }catch(const std::exception& e){
            LOG_ERROR<<"FastDFS upload error:"<<e.what();
        }
        try{
            if(!(co_await ordinary::RemoveOrdinaryFile(path))){
                LOG_ERROR<<"remove ordinary upload failed:"<<path;
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"remove ordinary upload error:"<<path<<" "<<e.what();
        }
        if(storageId.empty()){
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,"上传文件失败");
        }

        Json::Value meta;
        meta[FR_NAME]=file.getFileName();
        meta[FR_FATHER]=parentId;
        meta[FR_SIZE]=file.fileLength();
        meta[FR_LAST_MODIFY]=modifiedAt;
        meta[FR_PATH]=storageId;

        Json::Value res;
        try{
            res=co_await database::CreateFile(userId,std::move(meta));
        }catch(const std::exception& e){
            LOG_ERROR<<"create uploaded file error:"<<storageId<<" "<<e.what();
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_FAILED);
        }
        if(res[FR_ID]==-1){
            if(res[FR_ERROR]==errors::NAME_CONFLICT){
                try{
                    if(!(co_await FastDfsStorage::getInstance().deleteFile(storageId))){
                        LOG_ERROR<<"delete rejected upload failed:"<<storageId;
                    }
                }catch(const std::exception& e){
                    LOG_ERROR<<"delete rejected upload error:"<<storageId<<" "<<e.what();
                }
                co_return response::Failure(k409Conflict,errors::NAME_CONFLICT,US_MSG_ENTRY_CONFLICT);
            }
            // 通用数据库错误可能来自写入后的断线，保留对象供后续核对。
            LOG_ERROR<<"create uploaded file result uncertain:"<<storageId;
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_FAILED);
        }
        // 成功完成全部上传流程 告诉前端 同时前端应自行请求完整目录内容
        Json::Value data;
        data["id"]=res[FR_ID];
        co_return response::Success(data,HttpStatusCode::k200OK,"上传成功");
    }


    Task<HttpResponsePtr> InitChunkUpload(uint64_t userId,uint64_t parentId,
                                    const std::string& name,uint64_t size,uint64_t lastModified){
        // 总共有两次检查 一次是初始化的时候 检查是否有同名文件 一个是合并文件并完成最终上传的时候 再次检查
        // 初次检查没有同名目录 会继续尝试生成uploadid并尝试creattask 后者会把创建目录的工作一并做了
        if(FormatLastModified(lastModified).empty()){
            co_return response::Failure(k400BadRequest,errors::VALIDATION_ERROR,US_MSG_BAD_MODIFIED_TIME);
        }
        if(auto failure=co_await CheckChunkDestination(userId,parentId,name))co_return failure;
        Json::Value meta;
        meta["owner_id"]=userId;
        meta["parent_id"]=parentId;
        meta["name"]=name;
        meta["size"]=size;
        meta["last_modified"]=lastModified;
        // 前端按初始化响应中的 chunk_size 切片。
        meta["chunk_size"]=chunk_size;
        meta["total_chunks"]=size==0?1:1+(size-1)/chunk_size;// 向上取整
        constexpr int maxAttempts=3;
        for(int attempt=0;attempt<maxAttempts;++attempt){
            const auto uploadId=chunk::CreateUploadId();
            if(uploadId.empty())break;

            const auto res=co_await chunk::CreateTask(uploadId,meta);
            if(res["status"].asBool()){
                Json::Value data;
                data["upload_id"]=uploadId;
                data["chunk_size"]=chunk_size;
                data["total_chunks"]=meta["total_chunks"];
                co_return response::Success(data,k201Created);
            }
            if(res["error"]!=UTS_ID_CONFLICT){
                LOG_ERROR<<"create chunk upload task failed:"<<uploadId<<" "<<res["error"].asString();
                break;
            }
        }
        co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"初始化任务失败");
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
            co_return response::Failure(k400BadRequest,errors::VALIDATION_ERROR,"分片编号超出范围");
        }
        const auto chunkSize=metadata["chunk_size"].asUInt64();
        const auto size=metadata["size"].asUInt64();
        const auto expectedSize=index==totalChunks ? size-(index-1)*chunkSize : chunkSize;
        if(chunk.fileLength()!=expectedSize){
            co_return response::Failure(k400BadRequest,errors::VALIDATION_ERROR,"分片长度不正确");
        }

        using chunk::SaveChunkResult;
        switch(co_await chunk::SaveChunk(uploadId,index,chunk)){
            case SaveChunkResult::Saved:{
                Json::Value data;
                data["status"]=true;
                data["upload_id"]=uploadId;
                data["index"]=Json::UInt64(index);
                data["stored"]=true;
                co_return response::Success(data,k200OK,"保存成功");
            }
            case SaveChunkResult::MissingTask:
                co_return UploadTaskMissing();
            case SaveChunkResult::RedisError:
                co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"暂时无法记录分片");
            case SaveChunkResult::StorageError:
                co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_SAVE_CHUNK_FAILED);
        }
        co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_SAVE_CHUNK_FAILED);
    }

    Task<HttpResponsePtr> QueryUploadStatus(uint64_t userId,const std::string& uploadId){
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task))co_return failure;
        const auto& metadata=task["metadata"];
        if(metadata.isMember("cleanup") || metadata.isMember(UTS_FINALIZING_ID))co_return UploadTaskBusy();

        Json::Value data;
        data["upload_id"]=uploadId;
        data["parent_id"]=metadata["parent_id"].asString();
        data["name"]=metadata["name"];
        data["size"]=metadata["size"];
        data["last_modified"]=metadata["last_modified"];
        data["chunk_size"]=metadata["chunk_size"];
        data["total_chunks"]=metadata["total_chunks"];
        data["uploaded_chunks"]=task["uploaded_chunks"];
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
                co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_BAD_TASK_DATA);
            }
            co_await CleanupCompletedUpload(uploadId,fileId);
            Json::Value data;
            data["id"]=Json::UInt64(fileId);
            co_return response::Success(data);
        }
        if(metadata.isMember(UTS_FINALIZING_ID)){
            const auto storageId=metadata[UTS_FINALIZING_ID].asString();
            const auto entry=co_await database::FindFileByStorageId(userId,storageId);
            if(entry[FR_ID]==-1){
                co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"上传提交结果待确认");
            }
            co_await CleanupCompletedUpload(uploadId,entry[FR_ID].asUInt64());
            Json::Value data;
            data["id"]=entry[FR_ID];
            co_return response::Success(data);
        }

        const auto totalChunks=metadata["total_chunks"].asUInt64();
        const auto& received=task["uploaded_chunks"];
        if(received.size()!=totalChunks){
            co_return response::Failure(k409Conflict,"UPLOAD_CHUNKS_MISSING",US_MSG_CHUNKS_MISSING);
        }
        uint64_t expectedIndex=1;
        for(const auto& index:received){
            if(index.asUInt64()!=expectedIndex++){
                co_return response::Failure(k409Conflict,"UPLOAD_CHUNKS_MISSING",US_MSG_CHUNKS_MISSING);
            }
        }

        const auto parentId=metadata["parent_id"].asUInt64();
        const auto name=metadata["name"].asString();
        if(auto failure=co_await CheckChunkDestination(userId,parentId,name))co_return failure;
        const auto modifiedAt=FormatLastModified(metadata["last_modified"].asUInt64());
        if(modifiedAt.empty()){
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_BAD_TASK_DATA);
        }
        const auto size=metadata["size"].asUInt64();
        const auto path=co_await chunk::MergeChunks(uploadId,totalChunks,size);
        if(path.empty()){
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,"合并分片失败");
        }
        std::string storageId;
        try{
            storageId=co_await FastDfsStorage::getInstance().uploadFile(path);
        }catch(const std::exception& e){
            LOG_ERROR<<"FastDFS upload error:"<<uploadId<<" "<<e.what();
        }
        if(storageId.empty()){
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,US_MSG_FAILED);
        }
        if(!(co_await chunk::MarkTaskFinalizing(uploadId,storageId))){
            // Redis 可能已经写入标记，保留它引用的对象供后续核对。
            LOG_ERROR<<"unable to confirm upload finalizing marker:"<<uploadId<<" "<<storageId;
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"暂时无法提交上传任务");
        }

        Json::Value filemeta;
        filemeta[FR_NAME]=name;
        filemeta[FR_SIZE]=Json::UInt64(size);
        filemeta[FR_PATH]=storageId;
        filemeta[FR_LAST_MODIFY]=modifiedAt;
        filemeta[FR_FATHER]=Json::UInt64(parentId);
        Json::Value res;
        try{
            res=co_await database::CreateFile(userId,std::move(filemeta));
        }catch(const std::exception& e){
            LOG_ERROR<<"create merged file error:"<<uploadId<<" "<<storageId<<" "<<e.what();
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_FAILED);
        }
        if(res[FR_ID]==-1){
            if(res[FR_ERROR]==errors::NAME_CONFLICT){
                if(!(co_await chunk::ClearTaskFinalizing(uploadId,storageId))){
                    LOG_ERROR<<"unable to clear rejected upload marker:"<<uploadId<<" "<<storageId;
                    co_return response::Failure(k409Conflict,errors::NAME_CONFLICT,US_MSG_FILE_CONFLICT);
                }
                try{
                    if(!(co_await FastDfsStorage::getInstance().deleteFile(storageId))){
                        LOG_ERROR<<"delete rejected merged file failed:"<<storageId;
                    }
                }catch(const std::exception& e){
                    LOG_ERROR<<"delete rejected merged file error:"<<storageId<<" "<<e.what();
                }
                co_return response::Failure(k409Conflict,errors::NAME_CONFLICT,US_MSG_FILE_CONFLICT);
            }
            LOG_ERROR<<"create merged file result uncertain:"<<uploadId<<" "<<storageId;
            co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,US_MSG_FAILED);
        }
        co_await CleanupCompletedUpload(uploadId,res[FR_ID].asUInt64());
        Json::Value data;
        data["id"]=res[FR_ID];
        co_return response::Success(data);
    }

    Task<HttpResponsePtr> AskCancelUpload(uint64_t userId,std::string uploadId){
        const ChunkTaskGuard guard(uploadId);
        Json::Value task;
        if(auto failure=co_await LoadOwnedTask(userId,uploadId,task))co_return failure;
        const auto& metadata=task["metadata"];
        if(!guard)co_return UploadTaskBusy();
        if(metadata.isMember("cleanup") && metadata["cleanup"]!="cancel"){
            co_return response::Failure(k409Conflict,"UPLOAD_COMPLETED","上传任务已完成");
        }
        if(metadata.isMember(UTS_FINALIZING_ID))co_return UploadTaskBusy();
        const auto res=co_await chunk::RemoveTask(uploadId);
        if(!res["status"].asBool()){
            if(res["error"]==UTS_REDIS_ERR){
                co_return response::Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"暂时无法清理上传任务");
            }
            co_return response::Failure(k500InternalServerError,errors::INTERNAL_ERROR,"清理失败");
        }
        Json::Value data;
        data["status"]=true;
        co_return response::Success(data);
    }
}
