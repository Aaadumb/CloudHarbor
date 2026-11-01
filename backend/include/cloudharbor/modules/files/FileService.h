#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <optional>
using namespace drogon;

namespace file{
    namespace server{
        Task<HttpResponsePtr> QueryDirectory(uint64_t userId,
                std::optional<uint64_t> directoryId=std::nullopt,std::optional<std::string> path=std::nullopt);
        Task<HttpResponsePtr> CreateDirectory(uint64_t userId,uint64_t parentId,const std::string&name);
        Task<HttpResponsePtr> DeleteEntry(uint64_t userId,uint64_t entryId);
        Task<HttpResponsePtr> DownloadFile(uint64_t userId,uint64_t fileId);
    }
}