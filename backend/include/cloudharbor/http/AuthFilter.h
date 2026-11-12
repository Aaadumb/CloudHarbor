#pragma once

#include <drogon/drogon.h>

using namespace drogon;
class AuthFilter:public HttpFilter<AuthFilter>{
public:
    static constexpr char UserIdAttribute[]="userId";
    static constexpr char SessionIdAttribute[]="sessionId";
    static std::string ReadSessionId(const HttpRequestPtr& req);
    void doFilter(const HttpRequestPtr&req,
        FilterCallback&&fcb,
        FilterChainCallback&&fccb) override;
};
