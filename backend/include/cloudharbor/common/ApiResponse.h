#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include "AppError.h"
using namespace drogon;

namespace response{
    HttpResponsePtr Success(Json::Value data,std::string message="",HttpStatusCode status=k200OK);
    HttpResponsePtr Failure(const HttpStatusCode& status,const std::string &code,const std::string& message);
    bool setLoginCookie(HttpResponsePtr& response,const std::string& sid,bool persistent,int64_t ttlSeconds);
    bool clearLoginCookie(HttpResponsePtr& response);
}