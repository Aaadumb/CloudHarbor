#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <vector>

// 字段名
#define FR_ID "id"
#define FR_NAME "name"
#define FR_TYPE "type"
#define FR_FATHER "father"
#define FR_SIZE "size"
#define FR_LAST_MODIFY "last_modify"
#define FR_PATH "path"
#define FR_STATUS "status"
#define FR_DATA "data"
#define FR_ERROR "error"

// 条目类型
#define FR_FILE "file"
#define FR_FOLDER "folder"

// 错误与日志
#define FR_NO_ENTRY "NOTEXISTS"
#define FR_NO_ENTRY_OR_MISMATCH "NOTEXISTSORMATCH"
#define FR_DB_ERROR "MYSQLERROR"
#define FR_DB_LOG "mysql error:"

using namespace drogon;
using namespace drogon::orm;
using TransactionPtr=std::shared_ptr<Transaction>;
namespace file{
    namespace database{
        Task<Json::Value> GetEntry(uint64_t userId,uint64_t entryId);
        Task<Json::Value> GetEntries(uint64_t userId,const std::vector<uint64_t>&entrIds);
        Task<Json::Value> ListChildren(uint64_t userId,uint64_t parentId);
        Task<Json::Value> ListChildren(uint64_t userId,const std::string& path);
        Task<Json::Value> FindChildEntry(uint64_t userId,uint64_t parentId,const std::string& name);
        Task<Json::Value> FindFileByStorageId(uint64_t userId,const std::string& storageId);
        Task<uint64_t> CreateRoot(const TransactionPtr& tx,uint64_t userId);
        Task<Json::Value> CreateDirectory(uint64_t userId,Json::Value metaData);
        Task<Json::Value> CreateFile(uint64_t userId,Json::Value metadata);
        Task<Json::Value> DeleteEntries(const TransactionPtr&tx,uint64_t userId,const std::vector<uint64_t>& entryIds);
    }
}
