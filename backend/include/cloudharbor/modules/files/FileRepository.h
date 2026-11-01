#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <vector>
using namespace drogon;
using namespace drogon::orm;
namespace file{
    namespace database{
        Task<Json::Value> GetEntry(uint64_t userId,uint64_t entryId);
        Task<Json::Value> ListChildren(uint64_t userId,uint64_t parentId);
        Task<Json::Value> FindChildDirectory(uint64_t userId,uint64_t parentId,const std::string& name);
        Task<uint64_t> CreateRoot(const TransactionPtr& tx,uint64_t userId);
        Task<Json::Value> CreateDirectory(uint64_t userId,Json::Value metaData);
        Task<Json::Value> CreateFile(uint64_t userId,Json::Value metadata);
        Task<bool> DeleteEntries(const TransactionPtr&tx,uint64_t userId,const std::vector<uint64_t>& entryIds);
    }
}