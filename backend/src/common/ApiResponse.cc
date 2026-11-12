#include "ApiResponse.h"

#include <utility>

namespace response{
    namespace {
        constexpr char defaultErrorMessage[] = "服务出错";

        HttpResponsePtr MakeJsonResponse(HttpStatusCode status,const std::string& code,
                                         const std::string& message,Json::Value data){
            Json::Value json;
            json["code"]=code;
            json["message"]=message;
            json["data"]=std::move(data);
            auto response=HttpResponse::newHttpJsonResponse(std::move(json));
            response->setStatusCode(status);
            return response;
        }

        Cookie MakeLoginCookie(const std::string& name,const std::string& value){
            Cookie cookie(name,value);
            cookie.setPath("/");
            cookie.setHttpOnly(true);
            cookie.setSecure(true);
            cookie.setSameSite(Cookie::SameSite::kLax);
            return cookie;
        }

        Cookie ExpiredLoginCookie(const std::string& name){
            auto cookie=MakeLoginCookie(name,"deleted");
            cookie.setMaxAge(0);
            return cookie;
        }
    }

    HttpResponsePtr Success(){
        Json::Value data;
        data["status"]=true;
        return Success(std::move(data));
    }
    HttpResponsePtr Success(Json::Value data,HttpStatusCode status,std::string message){
        return MakeJsonResponse(status,"OK",message,std::move(data));
    }
    HttpResponsePtr SuccessWithId(Json::Value id,std::string message){
        Json::Value data;
        data["id"]=std::move(id);
        return Success(std::move(data),k200OK,std::move(message));
    }
    HttpResponsePtr Failure(const HttpStatusCode& status,const std::string &code,const std::string& message){
        return MakeJsonResponse(status,code,message,Json::Value(Json::nullValue));
    }
    HttpResponsePtr ValidationError(const std::string& message){
        return Failure(k400BadRequest,errors::VALIDATION_ERROR,message);
    }
    HttpResponsePtr AuthRequired(const std::string& message){
        return Failure(k401Unauthorized,errors::AUTH_REQUIRED,message);
    }
    HttpResponsePtr Forbidden(const std::string& message){
        return Failure(k403Forbidden,errors::FORBIDDEN,message);
    }
    HttpResponsePtr NotFound(const std::string& message){
        return Failure(k404NotFound,errors::NOT_FOUND,message);
    }
    HttpResponsePtr NameConflict(const std::string& message){
        return Failure(k409Conflict,errors::NAME_CONFLICT,message);
    }
    HttpResponsePtr InternalError(){
        return InternalError(defaultErrorMessage);
    }
    HttpResponsePtr InternalError(const std::string& message){
        return Failure(k500InternalServerError,errors::INTERNAL_ERROR,message);
    }
    HttpResponsePtr ServiceUnavailable(){
        return ServiceUnavailable(defaultErrorMessage);
    }
    HttpResponsePtr ServiceUnavailable(const std::string& message){
        return Failure(k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,message);
    }
    HttpResponsePtr RedirectPage(const std::string& location){
        auto response=HttpResponse::newRedirectionResponse(location,k302Found);
        response->addHeader("Cache-Control","no-store");
        return response;
    }
    void SetStorageRedirect(const HttpResponsePtr& response,const std::string& storageId){
        response->addHeader("X-Accel-Redirect","/_download/"+storageId);
        response->addHeader("Cache-Control","private, no-store");
    }
    void setLoginCookie(HttpResponsePtr& response,const std::string& sid,bool persistent,int64_t ttlSeconds){
        auto c=MakeLoginCookie(LoginCookieName,sid);
        if(persistent){
            c.setMaxAge(ttlSeconds);
        }
        response->addCookie(c);
        response->addCookie(ExpiredLoginCookie(LegacyLoginCookieName));
    }
    void clearLoginCookie(HttpResponsePtr& response){
        response->addCookie(ExpiredLoginCookie(LoginCookieName));
        response->addCookie(ExpiredLoginCookie(LegacyLoginCookieName));
    }
}
