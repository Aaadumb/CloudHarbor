#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include "VerificationCodeStore.h"
// #include <drogon/utils/coroutine.h>

// 字段名
#define UR_ID "id"
#define UR_NAME "name"
#define UR_MAIL "mail"
#define UR_PWD "password"
#define UR_ROOT "root_id"
#define UR_AVATAR "avatar_path"
#define UR_STATUS "status"

// 错误与日志
#define UR_ERROR "error"
#define UR_NO_USER "NOTEXISTS"
#define UR_DB_ERROR "MYSQLERROR"
#define UR_DB_LOG "mysql error:"

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
        Task<bool> BindRoot(const TransactionPtr& tx,uint64_t userId,uint64_t rootId);
        Task<bool> ModifyName(uint64_t userId,const std::string& name);
        Task<bool> ModifyMail(uint64_t userId,const std::string& oldmail,const std::string& newmail);
        Task<bool> ModifyPassword(uint64_t userId,const std::string& passwordHash);
        Task<bool> ModifyAvatar(uint64_t userId,const std::string& avatarPath);
        Task<bool> ModifyFiled(uint64_t userId,const std::string filed,const std::string& content);
    }
}
