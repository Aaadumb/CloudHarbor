#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <sys/random.h>
#include <chrono>
#include <cstdint>

#define SS_KEY "ch:session:"

using namespace drogon;
namespace account{
    namespace session{
        inline int64_t GetNow();        // 得到当前的时间戳
        std::string CreateSessionId();
        Task<std::string> CreateSession(uint64_t userId,uint64_t ttlSeconds,const std::string mode);
        Task<Json::Value> FindSession(const std::string& sid);
        Task<bool> RevokeSession(const std::string& sid);
        // Task<bool> RevokeUserSessions(uint64_t userId); 这个接口已确认无用
    }
}
