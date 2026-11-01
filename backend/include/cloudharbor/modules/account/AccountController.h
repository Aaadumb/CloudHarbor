#pragma once

#include <drogon/drogon.h>
#include <functional>
// #include <drogon/utils/coroutine.h>
#include "http/AuthFilter.h"
// #include "../../http/AuthFilter.h"
#include "AccountService.h"
using namespace account;
using namespace drogon;
class AccountController:public HttpController<AccountController>{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(AccountController::entry,"/",Get,"AuthFilter");
        ADD_METHOD_TO(AccountController::loginPage,"/login.html",Get);
        ADD_METHOD_TO(AccountController::indexPage,"/index.html",Get,"AuthFilter");
        ADD_METHOD_TO(AccountController::sendCode,"/account/send_mail",Post);
        ADD_METHOD_TO(AccountController::login,"/account/login",Post);
        ADD_METHOD_TO(AccountController::registerAccount,"/account/register",Post);
        ADD_METHOD_TO(AccountController::getAccountInfo,"/account/me",Get);
        ADD_METHOD_TO(AccountController::getAvatar,"/account/avatar",Get);
        ADD_METHOD_TO(AccountController::modifyPassword,"/account/password",Post);
        ADD_METHOD_TO(AccountController::modifyMail,"/account/mail",Post);
        ADD_METHOD_TO(AccountController::modifyName,"/account/name",Post);
        ADD_METHOD_TO(AccountController::modifyAvatar,"/account/avatar",Post);
        ADD_METHOD_TO(AccountController::logout,"/account/logout",Post);
    METHOD_LIST_END
private:
    Task<void> entry(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> loginPage(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> indexPage(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> sendCode(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> login(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> registerAccount(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> getAccountInfo(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> getAvatar(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> modifyPassword(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> modifyMail(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> modifyName(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> modifyAvatar(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> logout(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
};
