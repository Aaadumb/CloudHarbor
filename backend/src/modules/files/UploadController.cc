#include "UploadController.h"
#include "AuthFilter.h"
#include "RequestJson.h"
#include "NumericLimits.h"
Task<void> UploadController::uploadFile
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    // 都是经过filter筛选过的 保证req能对应到人
    // 需要解析文件 包含文件和parentid lastModified 以及file
    MultiPartParser parser;
    if(parser.parse(req)!=0){
        // 解析失败
        callback(response::ValidationError("文件解析出错"));
        co_return;
    }
    // 解析成功
    const auto& parentIdText=parser.getOptionalParameter<std::string>("parentId");
    const auto parentIdPtr=parentIdText ? request::ParseId(*parentIdText) : std::nullopt;
    const auto& lastModifiedText=parser.getOptionalParameter<std::string>("lastModified");
    const auto lastModifiedPtr=lastModifiedText
        ? request::ParseUnsigned(*lastModifiedText,numeric::MaxFileModifiedMilliseconds) : std::nullopt;
    if(!parentIdPtr||!lastModifiedPtr){
        callback(response::ValidationError("文件解析出错"));
        co_return;
    }
    uint64_t parentId=*parentIdPtr,lastModified=*lastModifiedPtr;
    const auto &files=parser.getFiles();
    if(files.empty()||files.size()>1){
        callback(response::ValidationError("文件解析出错"));
        co_return;
    }
    uint64_t userId=req->getAttributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await upload::UploadFile(userId,parentId,lastModified,files[0]));
    co_return;
}


Task<void> UploadController::initChunkUpload
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto& json=*jsonptr;
    if(!json.isMember("parentId")||!json.isMember("name")||!json.isMember("size")||!json.isMember("lastModified")){
        callback(response::ValidationError());
        co_return;
    }
    const auto parentId=request::ReadId(json["parentId"]);
    const auto size=request::ReadUnsigned(json["size"],numeric::MaxSafeInteger);
    const auto lastModified=request::ReadUnsigned(json["lastModified"],numeric::MaxFileModifiedMilliseconds);
    if(!parentId||!json["name"].isString()||!size||!lastModified){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->getAttributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    std::string name=json["name"].asString();
    callback(co_await upload::InitChunkUpload(userId,*parentId,name,*size,*lastModified));
    co_return;
}


Task<void> UploadController::uploadFileByChunk
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    MultiPartParser parser;
    if(parser.parse(req)!=0){
        callback(response::ValidationError("文件解析错误"));
        co_return;
    }
    // uploadid, index, chunk
    const auto& uploadIdPtr=parser.getOptionalParameter<std::string>("uploadId");
    const auto& indexText=parser.getOptionalParameter<std::string>("index");
    const auto indexPtr=indexText
        ? request::ParseUnsigned(*indexText,numeric::MaxSafeInteger) : std::nullopt;
    if(!uploadIdPtr||!indexPtr||*indexPtr==0){
        callback(response::ValidationError("文件解析错误"));
        co_return;
    }
    const auto& files=parser.getFiles();
    if(files.size()!=1){
        callback(response::ValidationError("文件解析出错"));
        co_return;
    }
    const auto& uploadId=*uploadIdPtr;
    const auto& index=*indexPtr;
    const auto& chunk=files[0];
    uint64_t userId=req->getAttributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await upload::UploadFileByChunk(userId,uploadId,index,chunk));
}


Task<void> UploadController::queryUploadStatus
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& params=req->getParameters();
    if(params.find("uploadId")==params.end()){
        callback(response::ValidationError());
        co_return;
    }
    std::string uploadId=params.find("uploadId")->second;
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    callback(co_await upload::QueryUploadStatus(userId,uploadId));
    co_return;
}


Task<void> UploadController::askMergeUpload
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto json=*jsonptr;
    if(!json.isMember("uploadId")||!json["uploadId"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    std::string uploadId=json["uploadId"].asString();
    callback(co_await upload::AskMergeUpload(userId,uploadId));
    co_return;
}


Task<void> UploadController::askCancelUpload
    (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback){
    const auto& jsonptr=request::ReadJsonObject(req);
    if(!jsonptr){
        callback(response::ValidationError());
        co_return;
    }
    const auto json=*jsonptr;
    if(!json.isMember("uploadId")||!json["uploadId"].isString()){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->attributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    std::string uploadId=json["uploadId"].asString();
    callback(co_await upload::AskCancelUpload(userId,uploadId));
    co_return;
}
