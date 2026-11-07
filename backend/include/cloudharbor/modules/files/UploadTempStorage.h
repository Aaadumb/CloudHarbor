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

namespace fs=std::filesystem;
using namespace drogon;

class CreateTaskDirectoryAwaiter:public CallbackAwaiter<bool>{
private:
    const std::string path;
    bool createTaskDirectory();
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
    namespace storage{
        std::string CreateUploadId(int n=16);
        Task<bool> CreateTaskDirectory(const std::string& uploadId);
        // 调用方校验任务归属、编号和长度，并保证同一任务的写片、合并和删除互斥。
        Task<bool> SaveChunk(const std::string& uploadId,uint64_t index,const HttpFile& chunk);
        Task<std::string> MergeChunks(const std::string& uploadId,uint64_t totalChunkes,uint64_t expectedSize);
        Task<bool> RemoveTaskDirectory(const std::string& uploadId);
        Task<Json::Value> CreateTask(const std::string& uploadId,Json::Value metadata);
        Task<Json::Value> GetTask(const std::string& uploadId);
        // 调用方验证归属后再清理。已开始的清理可以重试。
        Task<Json::Value> RemoveTask(const std::string& uploadId);
    }
}
