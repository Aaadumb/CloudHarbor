#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include "AppError.h"
// #include <Cookie.h>
#include <drogon/Cookie.h>
using namespace drogon;

namespace response{
    HttpResponsePtr Success();
    HttpResponsePtr Success(Json::Value data,HttpStatusCode status=k200OK,std::string message="");
    HttpResponsePtr SuccessWithId(Json::Value id,std::string message="");
    HttpResponsePtr Failure(const HttpStatusCode& status,const std::string &code,const std::string& message);
    HttpResponsePtr ValidationError(const std::string& message="校验失败");
    HttpResponsePtr AuthRequired(const std::string& message="cookie失效 请重新登录");
    HttpResponsePtr Forbidden(const std::string& message="错误操作");
    HttpResponsePtr NotFound(const std::string& message);
    HttpResponsePtr NameConflict(const std::string& message);
    HttpResponsePtr InternalError();
    HttpResponsePtr InternalError(const std::string& message);
    HttpResponsePtr ServiceUnavailable();
    HttpResponsePtr ServiceUnavailable(const std::string& message);
    void SetStorageRedirect(const HttpResponsePtr& response,const std::string& storageId);
    void setLoginCookie(HttpResponsePtr& response,const std::string& sid,bool persistent,int64_t ttlSeconds);
    void clearLoginCookie(HttpResponsePtr& response);
}
