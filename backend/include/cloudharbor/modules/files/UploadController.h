#pragma once
#include <drogon/drogon.h>
#include <functional>
using namespace drogon;
class UploadController:public HttpController<UploadController>{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UploadController::uploadFile,"/files/upload",Post);
        ADD_METHOD_TO(UploadController::initChunkUpload,"/files/upload/init",Post);
        ADD_METHOD_TO(UploadController::uploadFileByChunk,"/files/upload/chunk",Post);
        ADD_METHOD_TO(UploadController::queryUploadStatus,"/files/upload/status",Get);
        ADD_METHOD_TO(UploadController::askMergeUpload,"/files/upload/complete",Post);
        ADD_METHOD_TO(UploadController::askCancelUpload,"/files/upload/cancel",Post);
    METHOD_LIST_END
    Task<void> uploadFile       (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> initChunkUpload  (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> uploadFileByChunk(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> queryUploadStatus(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> askMergeUpload   (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> askCancelUpload  (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
};