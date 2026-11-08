#pragma once
#include <drogon/drogon.h>
#include "FastDfsStorage.h"
#include "FileRepository.h"
#include "ApiResponse.h"
#include "AppError.h"
#include "UploadTempStorage.h"
#include <json/json.h>
using namespace file;
using namespace drogon;
namespace upload{
    Task<HttpResponsePtr> UploadFile(uint64_t userId,uint64_t parentId,uint64_t lastModified,const HttpFile& file);
    // uploadId、chunkSize 和 totalChunks 由 Service 生成或计算。
    Task<HttpResponsePtr> InitChunkUpload(uint64_t userId,uint64_t parentId,const std::string& name,uint64_t size,uint64_t lastModified);
    Task<HttpResponsePtr> UploadFileByChunk(uint64_t userId,const std::string& uploadId,uint64_t index,const HttpFile&chunk);
    Task<HttpResponsePtr> QueryUploadStatus(uint64_t userId,const std::string& uploadId);
    Task<HttpResponsePtr> AskMergeUpload(uint64_t userId,const std::string&uploadId);
    Task<HttpResponsePtr> AskCancelUpload(uint64_t userId,std::string uploadId);
}
