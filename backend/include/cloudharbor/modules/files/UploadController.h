#pragma once
#include <drogon/drogon.h>
#include <functional>
#include "UploadService.h"
using namespace drogon;
class UploadController:public HttpController<UploadController>{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UploadController::uploadFile,"/files/upload",Post,"AuthFilter");
        ADD_METHOD_TO(UploadController::initChunkUpload,"/files/upload/init",Post,"AuthFilter");
        ADD_METHOD_TO(UploadController::uploadFileByChunk,"/files/upload/chunk",Post,"AuthFilter");
        ADD_METHOD_TO(UploadController::queryUploadStatus,"/files/upload/status",Get,"AuthFilter");
        ADD_METHOD_TO(UploadController::askMergeUpload,"/files/upload/complete",Post,"AuthFilter");
        ADD_METHOD_TO(UploadController::askCancelUpload,"/files/upload/cancel",Post,"AuthFilter");
    METHOD_LIST_END
private:
    Task<void> uploadFile       (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> initChunkUpload  (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> uploadFileByChunk(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> queryUploadStatus(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> askMergeUpload   (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> askCancelUpload  (HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
};
