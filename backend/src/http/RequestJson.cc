#include "RequestJson.h"
#include <charconv>
#include <limits>

namespace request{
    std::optional<uint64_t> ParseId(std::string_view value){
        const auto id=ParseUnsigned(value,std::numeric_limits<uint64_t>::max());
        if(!id || *id==0)return std::nullopt;
        return id;
    }

    std::optional<uint64_t> ParseUnsigned(std::string_view value,uint64_t maximum){
        if(value.empty())return std::nullopt;
        uint64_t id=0;
        const auto end=value.data()+value.size();
        const auto [ptr,error]=std::from_chars(value.data(),end,id);
        if(error!=std::errc{} || ptr!=end || id>maximum){
            return std::nullopt;
        }
        return id;
    }

    std::optional<uint64_t> ReadId(const Json::Value& value){
        if(value.isString())return ParseId(value.asString());
        // 兼容原有 JSON 整数请求，新客户端统一发送十进制字符串。
        const auto id=ReadUnsigned(value,std::numeric_limits<uint64_t>::max());
        if(!id || *id==0)return std::nullopt;
        return id;
    }

    std::optional<uint64_t> ReadUnsigned(const Json::Value& value,uint64_t maximum){
        if((value.type()==Json::intValue || value.type()==Json::uintValue)
                && value.isUInt64() && value.asUInt64()<=maximum){
            return value.asUInt64();
        }
        return std::nullopt;
    }

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
