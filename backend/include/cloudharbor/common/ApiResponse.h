#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include "AppError.h"
// #include <Cookie.h>
#include <drogon/Cookie.h>
using namespace drogon;

namespace response{
    HttpResponsePtr Success(Json::Value data,HttpStatusCode status=k200OK,std::string message="");
    HttpResponsePtr Failure(const HttpStatusCode& status,const std::string &code,const std::string& message);
    void setLoginCookie(HttpResponsePtr& response,const std::string& sid,int64_t ttlSeconds);
    void clearLoginCookie(HttpResponsePtr& response);
}