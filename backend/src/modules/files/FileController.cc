#include "FileController.h"

#include "AuthFilter.h"
#include "FileService.h"
#include "RequestJson.h"
#include <charconv>

namespace{
    std::optional<uint64_t> ParseEntryId(const std::string& value){
        uint64_t id=0;
        const auto end=value.data()+value.size();
        const auto [ptr,error]=std::from_chars(value.data(),end,id);
        if(error!=std::errc{} || ptr!=end || id==0){
            return std::nullopt;
        }
        return id;
    }
}

Task<void> FileController::queryDirectory
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& parameters=req->getParameters();
    const auto id=parameters.find("id");
    const auto path=parameters.find("path");
    if((id==parameters.end())==(path==parameters.end())){
        callback(response::ValidationError());
        co_return;
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    if(path!=parameters.end()){
        callback(co_await file::server::QueryDirectory(userId,std::nullopt,path->second));
        co_return;
    }

    const auto directoryId=ParseEntryId(id->second);
    if(!directoryId){
        callback(response::ValidationError());
        co_return;
    }
    callback(co_await file::server::QueryDirectory(userId,*directoryId));
}


Task<void> FileController::createDirectory
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto jsonptr=request::ReadJsonObject(req);
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
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::CreateDirectory(userId,json["parentId"].asUInt64(),json["name"].asString()));
}


Task<void> FileController::deleteEntry
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& ids=(*jsonptr)["entryIds"];
    if(!ids.isArray() || ids.empty()){
        callback(response::ValidationError());
        co_return;
    }

    std::vector<uint64_t> entryIds;
    entryIds.reserve(ids.size());
    for(const auto& id:ids){
        if(!id.isUInt64() || id.asUInt64()==0){
            callback(response::ValidationError());
            co_return;
        }
        entryIds.push_back(id.asUInt64());
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::DeleteEntries(userId,entryIds));
}


Task<void> FileController::downloadFile
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto fileId=ParseEntryId(req->getParameter("id"));
    if(!fileId){
        callback(response::ValidationError());
        co_return;
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::DownloadFile(userId,*fileId));
}
