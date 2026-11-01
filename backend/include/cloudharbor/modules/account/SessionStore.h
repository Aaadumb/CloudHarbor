#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
using namespace drogon;
namespace account{
    namespace session{
        std::string CreateSessionId();
        Task<std::string> CreateSession(uint64_t userId,uint64_t ttlSeconds);
        Task<Json::Value> FindSession(const std::string& sid);
        Task<bool> RevokeSession(const std::string& sid);
        Task<bool> RevokeUserSessions(uint64_t userId);
    }
}