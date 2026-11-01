#pragma once

#include <drogon/drogon.h>
#include <functional>
using namespace drogon;

class FileController:public HttpController<FileController>{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(FileController::queryDirectory,"/files/query",Get);
        ADD_METHOD_TO(FileController::createDirectory,"/files/create",Post);
        ADD_METHOD_TO(FileController::deleteEntry,"/files/delete",Delete);
        ADD_METHOD_TO(FileController::downloadFile,"/files/download",Get);
    METHOD_LIST_END
    Task<void> queryDirectory(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> createDirectory(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> deleteEntry(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> downloadFile(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
};