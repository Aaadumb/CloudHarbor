#pragma once

#include <curl/curl.h>
#include <drogon/drogon.h>
#include <cstring>
#include <string> 
#include <cstdio>
#include <regex>
#include <ctime>
#include <coroutine>

#include "ThreadPool.h"

#define SMTP_URL "smtps://smtp.qq.com:465"
using namespace drogon;
// 不出意外的话 采用单例模式
class MailAwaiter:public CallbackAwaiter<bool>{
private:
    std::string mail,code;
    const std::string smtpUser, smtpPassword;
    bool sendCodeBlock();
public:
    MailAwaiter(const std::string& m,const std::string& c);
    void await_suspend(std::coroutine_handle<> handle);
};
class MailSender{
public:
    static MailSender& getInstance();
    Task<bool> sendCode(const std::string& mail,const std::string& code);
    
    MailSender(const MailSender& o)=delete;
    MailSender& operator=(const MailSender& o)=delete;
private:
    // 放一些其他自用函数 和 配置变量
    bool checkMail(const std::string& mail);
    MailSender()=default;
    ~MailSender()=default;
};
