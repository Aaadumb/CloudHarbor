#pragma once
#include <drogon/drogon.h>
#include <sys/random.h>
#include <regex>
using namespace drogon;
namespace account{
    namespace mail{
        bool CreateCode(std::string& out,int len=6);
        bool CheckMail(const std::string& mail);
        Task<Json::Value> BindRegisterCode(const std::string& mail,const std::string& code);
        Task<Json::Value> ConsumeRegisterCode(const std::string& mail,const std::string& code);
        Task<bool> InvalidateRegisterCode(const std::string& mail,const std::string& code);
    }
}