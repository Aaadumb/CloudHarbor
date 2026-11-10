#include "ApiResponse.h"
namespace response{
    namespace {
        constexpr char defaultErrorMessage[] = "服务出错";
    }

    HttpResponsePtr Success(Json::Value data,HttpStatusCode status,std::string message){
        Json::Value json;
        json["code"]="OK";
        json["message"]=message;
        json["data"]=data;
        HttpResponsePtr req=HttpResponse::newHttpJsonResponse(json);
        req->setStatusCode(status);
        return req;
    }
    HttpResponsePtr Failure(const HttpStatusCode& status,const std::string &code,const std::string& message){
        Json::Value json;
        json["code"]=code;
        json["message"]=message;
        json["data"]=Json::Value(Json::nullValue);
        HttpResponsePtr req=HttpResponse::newHttpJsonResponse(json);
        req->setStatusCode(status);
        return req;
    }
    HttpResponsePtr InternalError(){
        return Failure(k500InternalServerError,errors::INTERNAL_ERROR,defaultErrorMessage);
    }
    HttpResponsePtr ServiceUnavailable(){
        return Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,defaultErrorMessage);
    }
    void setLoginCookie(HttpResponsePtr& response,const std::string& sid,bool persistent,int64_t ttlSeconds){
        Cookie c("session_id",sid);
        c.setPath("/");
        c.setHttpOnly(true);
        c.setSecure(true);
        c.setSameSite(Cookie::SameSite::kLax);
        if(persistent){
            c.setMaxAge(ttlSeconds);
        }
        response->addCookie(c);
    }
    void clearLoginCookie(HttpResponsePtr& response){
        Cookie c("session_id","deleted");
        c.setPath("/");
        c.setHttpOnly(true);
        c.setSecure(true);
        c.setSameSite(Cookie::SameSite::kLax);
        c.setMaxAge(0);
        response->addCookie(c);
    }
}
