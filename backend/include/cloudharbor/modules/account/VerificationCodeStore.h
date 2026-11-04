#pragma once
#include <drogon/drogon.h>
#include <sys/random.h>
#include <drogon/nosql/RedisClient.h>
#include <regex>

#define CH_REGISTER_CODE_KEY_PREFIX "ch:mail_code:register:"
#define CH_REGISTER_COOLDOWN_KEY_PREFIX "ch:mail_cooldown:register:"
#define CH_VERIFICATION_CODE_FIELD "code"
#define CH_VERIFICATION_ATTEMPTS_FIELD "remaining_attempts"
#define CH_VERIFICATION_VALID_FIELD "valid"
#define CH_VERIFICATION_EXPIRED "VERIFICATION_EXPIRED"
#define CH_VERIFICATION_INVALID "VERIFICATION_INVALID"
#define CH_VERIFICATION_UNKNOWN_ERROR "UNKNOWN_ERROR"
#define CH_VERIFICATION_OK "OK"
#define CH_REDIS_EXISTS_COMMAND "exists %s"
#define CH_REDIS_ERROR_PREFIX "redis error:"

using namespace drogon;
namespace account{
    namespace mail{
        bool CreateCode(std::string& out,int len=6);
        bool CheckMail(const std::string& mail);
        Task<bool> ExistMail(const std::string& mail);
        Task<bool> IsMailCooling(const std::string& mail);
        Task<bool> InvalidateRegisterCode(const std::string& mail);
        Task<bool> BindRegisterCode(const std::string& mail,const std::string& code);
        Task<Json::Value> ConsumeRegisterCode(const std::string& mail,const std::string& code);
    }
}
