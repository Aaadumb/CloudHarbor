#pragma once

#include <drogon/drogon.h>
#include <functional>
// #include <drogon/utils/coroutine.h>
#include <drogon/MultiPart.h>

#include "UserRepository.h"
#include "SessionStore.h"
#include "VerificationCodeStore.h"
#include "ApiResponse.h"
#include "PasswordHasher.h"
#include "MailSender.h"
#include "FileRepository.h"
using namespace account;
using namespace drogon;
namespace account{
    namespace server{
        Task<HttpResponsePtr> SendCode(const std::string &mail);
        Task<HttpResponsePtr> Login(const std::string& auth,const std::string& pwd,bool persistent);
        Task<HttpResponsePtr> RegisterAccount(const std::string& name,const std::string& mail,
                                                const std::string& code,const std::string& pwd,bool persistent);
        Task<HttpResponsePtr> GetAccountInfo(uint64_t userId);  
        Task<HttpResponsePtr> GetAvatar(uint64_t userId);       
        Task<HttpResponsePtr> ModifyName(uint64_t userId,const std::string name);
        Task<HttpResponsePtr> ModifyAvatar(uint64_t userId,const HttpFile& file);      
        Task<HttpResponsePtr> ModifyMail(uint64_t userId,const std::string oldmail,
                                            const std::string newmail,const std::string oldcode,
                                            const std::string newcode);
        Task<HttpResponsePtr> ModifyPassword(uint64_t userId,const std::string& oldpwd,const std::string& newpwd);
        Task<HttpResponsePtr> Logout(const std::string& sid);
    }
};