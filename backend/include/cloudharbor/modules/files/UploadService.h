#pragma once
#include <drogon/drogon.h>
using namespace drogon;
namespace upload{
    Task<HttpResponsePtr> UploadFile(uint64_t userId,uint64_t parentId,uint64_t lastModified,const HttpFile& file);
    Task<HttpResponsePtr> InitChunkUpload(uint64_t userId,uint64_t parentId,uint64_t lastModified,const std::string& name,uint64_t size);
    Task<HttpResponsePtr> UploadFileByChunk(uint64_t userId,const std::string& uploadId,uint64_t index,const HttpFile&chunk);
    Task<HttpResponsePtr> QueryUploadStatus(uint64_t userId,const std::string& uploadId);
    Task<HttpResponsePtr> AskMergeUpload(uint64_t userId,const std::string&uploadId);
    Task<HttpResponsePtr> AskCancelUpload(uint64_t userId,std::string uploadId);
}