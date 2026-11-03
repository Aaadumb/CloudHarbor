#pragma once
#include <drogon/drogon.h>
#include <sys/random.h>
using namespace drogon;
namespace account{
    namespace mail{
        bool CreateCode(std::string& out,int len=6);
        Task<Json::Value> BindRegisterCode(const std::string& mail,const std::string& code);
        Task<Json::Value> ConsumeRegisterCode(const std::string& mail,const std::string& code);
        Task<bool> InvalidateRegisterCode(const std::string& mail,const std::string& code);
    }
}