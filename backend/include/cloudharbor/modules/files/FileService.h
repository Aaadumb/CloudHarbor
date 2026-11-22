#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <optional>
#include "FileRepository.h"
#include "ApiResponse.h"
#include "FastDfsStorage.h"
using namespace drogon;

namespace file{
    namespace server{
        Task<HttpResponsePtr> QueryDirectory(uint64_t userId,
                std::optional<uint64_t> directoryId=std::nullopt,std::optional<std::string> path=std::nullopt);
        Task<HttpResponsePtr> CreateDirectory(uint64_t userId,uint64_t parentId,const std::string&name);
        Task<HttpResponsePtr> DeleteEntries(uint64_t userId,const std::vector<uint64_t>& entryIds);
        Task<HttpResponsePtr> DownloadFile(uint64_t userId,uint64_t fileId);
        Task<HttpResponsePtr> getDownloadFileUrl(uint64_t userId,uint64_t id);
        Task<HttpResponsePtr> MoveEntries(uint64_t userId,const std::vector<uint64_t>& entryIds,uint64_t targetId);
        Task<HttpResponsePtr> CopyEntries(uint64_t userId,const std::vector<uint64_t>& entryIds,uint64_t targetId);
        Task<void> CleanupStorageObjects();
    }
}
