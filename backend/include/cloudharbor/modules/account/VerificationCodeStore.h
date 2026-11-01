#pragma once
#include <drogon/drogon.h>
using namespace drogon;
namespace account{
    namespace mail{
        std::string CreateCode();
        Task<Json::Value> BindRegisterCode(const std::string& mail,const std::string& code);
        Task<Json::Value> ConsumeRegisterCode(const std::string& mail,const std::string& code);
        Task<bool> InvalidateRegisterCode(const std::string& mail,const std::string& code);
    }
}