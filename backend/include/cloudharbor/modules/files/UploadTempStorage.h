#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <sys/random.h>
#include <fstream>
#include <filesystem>
#include "ThreadPool.h"
#include <trantor/utils/Logger.h>
#define UTS_PATH_ROOT "runtime/uploads/"
#define UTS_KEY "ch:upload:"
#define UTS_BAD_ID "INVALIDUPLOADID"
#define UTS_REDIS_ERR "REDISERROR"
#define UTS_ID_CONFLICT "KEYEXISTS"
#define UTS_NO_TASK "NOEXISTS"
#define UTS_FINALIZING_ID "finalizing_storage_id"

namespace fs=std::filesystem;
using namespace drogon;

enum class TaskDirectoryResult{Failed,Created,Exists};

class CreateTaskDirectoryAwaiter:public CallbackAwaiter<TaskDirectoryResult>{
private:
    const std::string path;
    TaskDirectoryResult createTaskDirectory();
public:
    CreateTaskDirectoryAwaiter(const std::string&uploadId);
    void await_suspend(std::coroutine_handle<> handle);
};

class SaveChunkAwaiter:public CallbackAwaiter<bool>{
private:
    const std::string path;
    uint64_t index;
    const HttpFile chunk;
    bool saveChunk();
public:
    SaveChunkAwaiter(const std::string& uploadId,uint64_t idx,const HttpFile&file);
    void await_suspend(std::coroutine_handle<> handle);
};

class MergeChunksAwaiter:public CallbackAwaiter<std::string>{
private:
    const std::string path;
    uint64_t totalChunkes,expectedSize;
    std::string mergeChunks();
public:
    MergeChunksAwaiter(const std::string& uploadId,uint64_t total,uint64_t size);
    void await_suspend(std::coroutine_handle<> handle);
};
class RemoveTaskDirectoryAwaiter:public CallbackAwaiter<bool>{
private:
    const std::string path;
    bool removeTaskDirectory();
public:
    RemoveTaskDirectoryAwaiter(const std::string &uploadId);
    void await_suspend(std::coroutine_handle<> handle);
};

namespace upload{
    Task<std::string> HashStagedFile(const std::string& path);
    namespace ordinary{
        // 写入并关闭文件后返回路径，保存失败返回空字符串。
        Task<std::string> SaveOrdinaryFile(HttpFile file);
        // 只接受 SaveOrdinaryFile 返回的路径，文件和目录已不存在时也返回 true。
        Task<bool> RemoveOrdinaryFile(std::string tempPath);
    }
    namespace chunk{
        enum class SaveChunkResult{Saved,StorageError,RedisError,MissingTask};
        std::string CreateUploadId(int n=16);
        Task<bool> CreateTaskDirectory(const std::string& uploadId);
        // 调用方校验任务归属、编号和长度，并保证同一任务的写片、合并和删除互斥。
        Task<SaveChunkResult> SaveChunk(const std::string& uploadId,uint64_t index,const HttpFile& chunk);
        Task<std::string> MergeChunks(const std::string& uploadId,uint64_t totalChunkes,uint64_t expectedSize);
        Task<bool> RemoveTaskDirectory(const std::string& uploadId);
        Task<Json::Value> CreateTask(const std::string& uploadId,Json::Value metadata);
        Task<Json::Value> GetTask(const std::string& uploadId);
        Task<bool> MarkTaskFinalizing(const std::string& uploadId,const std::string& storageId);
        Task<bool> ClearTaskFinalizing(const std::string& uploadId,const std::string& storageId);
        // 调用方验证归属后再清理。已开始的清理可以重试。
        Task<Json::Value> RemoveTask(const std::string& uploadId,std::string completedFileId={});
    }
}
