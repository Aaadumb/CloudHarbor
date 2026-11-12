#pragma once

#include <drogon/HttpRequest.h>

namespace request{
    std::shared_ptr<const Json::Value> ReadJsonObject(const drogon::HttpRequestPtr& req);
}
