#include "UploadController.h"
#include "AuthFilter.h"
#include "RequestJson.h"
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
    const auto& parentIdPtr=parser.getOptionalParameter<uint64_t>("parentId");
    const auto& lastModifiedPtr=parser.getOptionalParameter<uint64_t>("lastModified");
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
    if(!json["parentId"].isUInt64()||!json["name"].isString()||!json["size"].isUInt64()||!json["lastModified"].isUInt64()){
        callback(response::ValidationError());
        co_return;
    }
    uint64_t userId=req->getAttributes()->get<uint64_t>(AuthFilter::UserIdAttribute);
    uint64_t parentId=json["parentId"].asUInt64(),size=json["size"].asUInt64(),lastModified=json["lastModified"].asUInt64();
    std::string name=json["name"].asString();
    callback(co_await upload::InitChunkUpload(userId,parentId,name,size,lastModified));
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
    const auto& indexPtr=parser.getOptionalParameter<std::uint64_t>("index");
    if(!uploadIdPtr||!indexPtr){
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
