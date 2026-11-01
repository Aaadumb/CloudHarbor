#pragma once

#include <drogon/drogon.h>
#include <sodium.h>
using namespace drogon;

class PasswordHasher{
public:
    PasswordHasher();
    Task<std::string> hashPassword(const std::string& pwd);
    Task<bool> verifyPassword(const std::string& pwd,const std::string& passwordHash);
    ~PasswordHasher();
private:
    // 一些自用函数和变量
};