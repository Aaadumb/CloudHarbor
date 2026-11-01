#pragma once

#include <curl/curl.h>
#include <time.h>
#include <drogon/drogon.h>
using namespace drogon;

class MailSender{
public:
    MailSender();
    Task<bool> sendCode(const std::string& mail,const std::string& code);
    ~MailSender();
private:
    // 放一些其他自用函数 和 配置变量
};