#include "AuthFilter.h"

#include "ApiResponse.h"
#include "SessionStore.h"
#include <exception>
#include <utility>

namespace{
    HttpResponsePtr RejectUnauthenticated(const HttpRequestPtr& req){
        if(req->path()=="/index.html"){
            return response::RedirectPage("/login.html");
        }
        return response::AuthRequired();
    }

    AsyncTask Authenticate(HttpRequestPtr req,FilterCallback reject,FilterChainCallback next){
        const auto sessionId=AuthFilter::ReadSessionId(req);
        if(sessionId.empty()){
            reject(RejectUnauthenticated(req));
            co_return;
        }

        HttpResponsePtr failure;
        try{
            const auto session=co_await account::session::FindSession(sessionId);
            if(session.isObject() && session["user_id"].isUInt64() &&
               session["user_id"].asUInt64()!=0){
                req->attributes()->insert(AuthFilter::UserIdAttribute,session["user_id"].asUInt64());
                req->attributes()->insert(AuthFilter::SessionIdAttribute,sessionId);
            }else if(session.isObject() && session["error"]=="NOEXISTS"){
                failure=RejectUnauthenticated(req);
            }else{
                failure=response::ServiceUnavailable();
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"session lookup failed: "<<e.what();
            failure=response::ServiceUnavailable();
        }catch(...){
            LOG_ERROR<<"session lookup failed";
            failure=response::ServiceUnavailable();
        }

        if(failure){
            reject(failure);
            co_return;
        }
        next();
    }
}

std::string AuthFilter::ReadSessionId(const HttpRequestPtr& req){
    const auto& sessionId=req->getCookie(response::LoginCookieName);
    return sessionId.empty() ? req->getCookie(response::LegacyLoginCookieName) : sessionId;
}

void AuthFilter::doFilter(const HttpRequestPtr& req,FilterCallback&& fcb,
                          FilterChainCallback&& fccb){
    Authenticate(req,std::move(fcb),std::move(fccb));
}
