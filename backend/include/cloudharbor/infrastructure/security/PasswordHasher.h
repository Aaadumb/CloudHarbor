#pragma once

#include <drogon/drogon.h>
#include <sodium.h>
#include <coroutine>
#include "ThreadPool.h"
using namespace drogon;

// 也是采用单例模式 全局唯一

class HasherAwaiter:public CallbackAwaiter<std::string>{
private:
    std::string pwd;
    std::string hashPassword();
public:
    HasherAwaiter(const std::string& p);
    ~HasherAwaiter()=default;
    void await_suspend(std::coroutine_handle<> handle);
};


class VerifyAwaiter:public CallbackAwaiter<bool>{
private:
    std::string pwd,hash;
    bool verifyPassword();
public:
    VerifyAwaiter(const std::string&p,const std::string& h);
    ~VerifyAwaiter()=default;
    void await_suspend(std::coroutine_handle<> handle);
};

class PasswordHasher{
public:
    static PasswordHasher& getInstance();
    Task<std::string> hashPassword(const std::string& pwd);
    Task<bool> verifyPassword(const std::string& pwd,const std::string& passwordHash);
    
    PasswordHasher(const PasswordHasher&x)=delete;
    PasswordHasher& operator=(const PasswordHasher&x)=delete;
private:
    // 一些自用函数和变量
    PasswordHasher()=default;
    ~PasswordHasher()=default;
};