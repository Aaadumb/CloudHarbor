#pragma once

#include <drogon/HttpRequest.h>
#include <cstdint>
#include <optional>
#include <string_view>

namespace request{
    std::shared_ptr<const Json::Value> ReadJsonObject(const drogon::HttpRequestPtr& req);
    std::optional<uint64_t> ParseId(std::string_view value);
    std::optional<uint64_t> ReadId(const Json::Value& value);
    std::optional<uint64_t> ParseUnsigned(std::string_view value,uint64_t maximum);
    std::optional<uint64_t> ReadUnsigned(const Json::Value& value,uint64_t maximum);
}
