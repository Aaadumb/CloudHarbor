#pragma once

#include <drogon/drogon.h>
#include <functional>
using namespace drogon;

class FileController:public HttpController<FileController>{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(FileController::queryDirectory,"/files/query",Get,"AuthFilter");
        ADD_METHOD_TO(FileController::createDirectory,"/files/create",Post,"AuthFilter");
        ADD_METHOD_TO(FileController::deleteEntry,"/files/delete",Delete,"AuthFilter");
        ADD_METHOD_TO(FileController::downloadFile,"/files/download",Get,"AuthFilter");
        ADD_METHOD_TO(FileController::getDownloadFileUrl,"/files/download",Post,"AuthFilter");
        ADD_METHOD_TO(FileController::moveEntries,"/files/move",Post,"AuthFilter");
        ADD_METHOD_TO(FileController::copyEntries,"/files/copy",Post,"AuthFilter");
    METHOD_LIST_END
private:
    Task<void> queryDirectory(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> createDirectory(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> deleteEntry(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> downloadFile(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> getDownloadFileUrl(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> moveEntries(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
    Task<void> copyEntries(HttpRequestPtr req,std::function<void(const HttpResponsePtr&)> callback);
};
