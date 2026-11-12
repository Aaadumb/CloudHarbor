#include "AccountController.h"
#include "ApiResponse.h"
#include "SessionStore.h"
#include "RequestJson.h"
Task<void> AccountController::entry
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 检查cookie 有就跳转到index 没有就跳转到login
    const auto cookie=AuthFilter::ReadSessionId(req);
    if(cookie.empty()){
        callback(response::RedirectPage("/login.html"));
        co_return;
    }
    // 校验cookie
    const auto res=co_await account::session::FindSession(cookie);
    if(!res.isObject() || !res["user_id"].isUInt64() || res["user_id"].asUInt64()==0){
        if(res.isObject() && res["error"]=="NOEXISTS"){
            callback(response::RedirectPage("/login.html"));
        }else{
            callback(response::ServiceUnavailable());
        }
        co_return;
    }
    // 能查到对应的人 让他跳转到index
    callback(response::RedirectPage("/index.html"));
    co_return;
}


Task<void> AccountController::loginPage
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 直接跳转即可
    HttpResponsePtr resp=HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpStatusCode::k200OK);
    resp->addHeader("X-Accel-Redirect","/_pages/login.html");
    callback(resp);
    co_return;
}


Task<void> AccountController::indexPage
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 能被authfilter筛过来的都是找得到的 直接跳转就行了
    HttpResponsePtr resp=HttpResponse::newHttpResponse();
    resp->setStatusCode(HttpStatusCode::k200OK);
    resp->addHeader("X-Accel-Redirect","/_pages/index.html");
    callback(resp);
    co_return;
}


Task<void> AccountController::sendCode
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    if(!json.isMember("mail")||!json["mail"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    std::string mail=json["mail"].asString();
    callback(co_await account::server::SendCode(mail));
    co_return;
}


Task<void> AccountController::login
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    if(!json.isMember("auth")||!json.isMember("pwd")){
        callback(response::ValidationError());
        co_return;
    }
    if(!json["auth"].isString()||!json["pwd"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    std::string preference=req->getCookie("cookiePreference");
    bool persistent=preference=="accept";
    callback(co_await account::server::Login(json["auth"].asString(),json["pwd"].asString(),persistent));
    co_return;
}


Task<void> AccountController::registerAccount
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    // name mail code pwd
    if(!json.isMember("name")||!json.isMember("mail")||!json.isMember("code")||!json.isMember("pwd")){
        callback(response::ValidationError());
        co_return;
    }
    if(!json["name"].isString()||!json["mail"].isString()||!json["code"].isString()||!json["pwd"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    std::string preference=req->getCookie("cookiePreference");
    bool persistent=preference=="accept";
    std::string name=json["name"].asString(),mail=json["mail"].asString();
    std::string code=json["code"].asString(),pwd=json["pwd"].asString();
    callback(co_await account::server::RegisterAccount(name,mail,code,pwd,persistent));
    co_return;
}


Task<void> AccountController::getAccountInfo
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 主要是把userid拿出来
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await account::server::GetAccountInfo(userId));
    co_return;
}


Task<void> AccountController::getAvatar
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await account::server::GetAvatar(userId));
    co_return;
}


Task<void> AccountController::modifyPassword
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto &jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto&json=*jsonptr;
    if(!json.isMember("oldPwd")||!json.isMember("newPwd")){
        callback(response::ValidationError());
        co_return;
    }
    if(!json["oldPwd"].isString()||!json["newPwd"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    std::string oldPwd=json["oldPwd"].asString();
    std::string newPwd=json["newPwd"].asString();
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await account::server::ModifyPassword(userId,oldPwd,newPwd));
    co_return;
}


Task<void> AccountController::modifyMail
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto &jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto&json=*jsonptr;
    if(!json.isMember("oldMail")||!json.isMember("newMail")||!json.isMember("oldCode")||!json.isMember("newCode")){
        callback(response::ValidationError());
        co_return;
    }
    if(!json["oldMail"].isString()||!json["newMail"].isString()||!json["oldCode"].isString()||!json["newCode"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    std::string newMail=json["newMail"].asString(),oldMail=json["oldMail"].asString();
    std::string newCode=json["newCode"].asString(),oldCode=json["oldCode"].asString();
    callback(co_await account::server::ModifyMail(userId,oldMail,newMail,oldCode,newCode));
    co_return;
}


Task<void> AccountController::modifyName
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto &jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto&json=*jsonptr;
    if(!json.isMember("name")||!json["name"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    std::string name=json["name"].asString();
    callback(co_await account::server::ModifyName(userId,name));
    co_return;
}


Task<void> AccountController::modifyAvatar
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    MultiPartParser parser;
    if(parser.parse(req)!=0){
        callback(response::ValidationError("文件解析错误"));
        co_return;
    }
    const auto& file=parser.getFiles();
    if(file.size()!=1){
        callback(response::ValidationError("文件解析错误"));
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await account::server::ModifyAvatar(userId,file[0]));
    co_return;
}


Task<void> AccountController::logout
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto cookie=AuthFilter::ReadSessionId(req);
    callback(co_await account::server::Logout(cookie));
    co_return;
}
