#include "RequestJson.h"

namespace request{
    std::shared_ptr<const Json::Value> ReadJsonObject(const drogon::HttpRequestPtr& req){
        if(req->getContentType()!=drogon::CT_APPLICATION_JSON){
            return nullptr;
        }
        const auto& json=req->getJsonObject();
        if(!json || !json->isObject()){
            return nullptr;
        }
        return json;
    }
}
