#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
// #include <drogon/utils/coroutine.h>
using namespace drogon;
using namespace drogon::orm;
namespace account{
    namespace database{
        Task<Json::Value> FindByAuth(const std::string& auth);
        Task<Json::Value> GetAccountInfo(uint64_t userId);
        Task<bool> IsNameUsed(const std::string& name);
        Task<bool> IsMailUsed(const std::string& mail);
        Task<uint64_t> CreateAccount(const TransactionPtr& tx,
                                    const std::string& name,const std::string&mail,
                                    const std::string& passwordHash);
        Task<void> BindRoot(const TransactionPtr& tx,uint64_t userId,uint64_t rootId);
        Task<bool> ModifyName(uint64_t userId,const std::string& name);
        Task<bool> ModifyMail(uint64_t userId,const std::string& mail);
        Task<bool> ModifyPassword(uint64_t userId,const std::string& passwordHash);
        Task<bool> ModifyAvatar(uint64_t userId,const std::string& avatarPath);
    }
}