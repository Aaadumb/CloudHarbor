#include "FileController.h"
#include <optional>

#include "AuthFilter.h"
#include "FileService.h"
#include "RequestJson.h"

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

    const auto directoryId=request::ParseId(id->second);
    if(!directoryId){
        callback(response::ValidationError());
        co_return;
    }
    callback(co_await file::server::QueryDirectory(userId,*directoryId));
    co_return;
}


Task<void> FileController::createDirectory
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    const auto parentId=request::ReadId(json["parentId"]);
    if(!parentId || !json["name"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::CreateDirectory(userId,*parentId,json["name"].asString()));
    co_return;
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
        const auto entryId=request::ReadId(id);
        if(!entryId){
            callback(response::ValidationError());
            co_return;
        }
        entryIds.push_back(*entryId);
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::DeleteEntries(userId,entryIds));
    co_return;
}


Task<void> FileController::downloadFile
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto fileId=request::ParseId(req->getParameter("id"));
    if(!fileId){
        callback(response::ValidationError());
        co_return;
    }
    const auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::DownloadFile(userId,*fileId));
    co_return;
}

Task<void> FileController::getDownloadFileUrl
     (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto &json=*jsonptr;
    const auto id=request::ReadId(json["id"]);
    if(!id){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::getDownloadFileUrl(userId,*id));
    co_return;
}
namespace {
    struct TransferRequest{
        uint64_t targetId;
        std::vector<uint64_t> entryIds;
    };

    std::optional<TransferRequest> ReadTransferRequest(const HttpRequestPtr& req){
        const auto json=request::ReadJsonObject(req);
        if(!json)return std::nullopt;
        const auto& ids=(*json)["entryIds"];
        const auto& target=(*json)["targetId"];
        if(!target.isString() || !ids.isArray() || ids.empty())return std::nullopt;
        const auto targetId=request::ReadId(target);
        if(!targetId)return std::nullopt;

        TransferRequest result{*targetId,{}};
        result.entryIds.reserve(ids.size());
        for(const auto& item:ids){
            const auto id=request::ReadId(item);
            if(!id)return std::nullopt;
            result.entryIds.push_back(*id);
        }
        return result;
    }
}

Task<void> FileController::moveEntries
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto transfer=ReadTransferRequest(req);
    if(!transfer){
        callback(response::ValidationError());
        co_return;
    }
    auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::MoveEntries(userId,transfer->entryIds,transfer->targetId));
}

Task<void> FileController::copyEntries
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto transfer=ReadTransferRequest(req);
    if(!transfer){
        callback(response::ValidationError());
        co_return;
    }
    auto userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await file::server::CopyEntries(userId,transfer->entryIds,transfer->targetId));
}
