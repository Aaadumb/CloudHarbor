#include "FileController.h"

namespace{
    std::shared_ptr<Json::Value> ReadJsonObject(const HttpRequestPtr& req){
        const auto& json=req->getJsonObject();
        if(!json || !json->isObject() || req->getHeader("Content-Type")!="application/json"){
            return nullptr;
        }
        return json;
    }

    Task<HttpResponsePtr> FindSessionUser(const std::string& sessionId,uint64_t& userId){
        const auto session=co_await account::session::FindSession(sessionId);
        if(session["userId"]==-1){
            if(session["error"]=="NOEXISTS"){
                co_return response::AuthRequired();
            }
            co_return response::ServiceUnavailable();
        }
        userId=session["userId"].asUInt64();
        co_return nullptr;
    }
}

Task<void> FileController::queryDirectory
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 主要是要拿到userid 和 path/storageid
    std::string cookie= req->getCookie("session_id");
    if(cookie.empty()){
        callback(response::AuthRequired());
        co_return;
    }
    const auto jsonptr=ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    if(json.isMember("path")+json.isMember("id")!=1){
        callback(response::ValidationError());
        co_return;
    }
    // 保证只有一个字段 现在开始判断类型
    if((json.isMember("path")&&!json["path"].isString())||(json.isMember("id")&&!json["id"].isUInt64())){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userid=0;
    if(auto failure=co_await FindSessionUser(cookie,userid)){
        callback(failure);
        co_return;
    }
    HttpResponsePtr resp;
    if(json.isMember("path"))resp=co_await file::server::QueryDirectory(userid,std::nullopt,json["path"].asString());
    else resp=co_await file::server::QueryDirectory(userid,json["id"].asUInt64());
    callback(resp);
    co_return;
}


Task<void> FileController::createDirectory
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    std::string cookie=req->getCookie("session_id");
    if(cookie.empty()){
        callback(response::AuthRequired());
        co_return;
    }
    const auto jsonptr=ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    if(!json.isMember("parentId") || !json.isMember("name")
        || !json["parentId"].isUInt64() || !json["name"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    // 检查一下cookie 能否对应到用户
    uint64_t userid=0;
    if(auto failure=co_await FindSessionUser(cookie,userid)){
        callback(failure);
        co_return;
    }
    // 参数齐全 开始执行函数
    HttpResponsePtr resp=
        co_await file::server::CreateDirectory(userid,json["parentId"].asUInt64(),json["name"].asString());
    callback(resp);
    co_return;
}


Task<void> FileController::deleteEntry
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){

}


Task<void> FileController::downloadFile
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){

}
