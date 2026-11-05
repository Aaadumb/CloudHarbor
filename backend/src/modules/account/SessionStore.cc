#include "SessionStore.h"
namespace account::session{
    std::string CreateSessionId(){
        // 32字节安全随机数编码为64个十六进制字符 出错时返回空字符串
        constexpr char hex[]="0123456789abcdef";
        std::string res;
        res.reserve(64);
        while(res.size()<64){
            unsigned char tmp;
            ssize_t n=getrandom(&tmp,sizeof tmp,0);
            if(n!=1){
                if(n<0 && errno==EINTR)continue;
                res.clear();
                break;
            }else{
                res+=hex[tmp>>4];
                res+=hex[tmp&0x0f];
            }
        }
        return res;
    }
    Task<std::string> CreateSession(uint64_t userId,uint64_t ttlSeconds,const std::string mode){
        // 最后尝试三次 如果三次全部失败 返回空字符串
        int maxTry=3;
        auto redis=app().getRedisClient();
        std::string sid;
        while(maxTry--){
            sid=CreateSessionId();
            if(sid.empty())continue;
            // nosql -> set ch:session:<%s> {"userId":%llu,"mode":"%s","createTime":%lld} NX EX %llu
            try{
                auto result=
                    co_await redis->execCommandCoro(
                        "set " SS_KEY "%s "
                        "{\"" SS_UID "\":%llu,\"" SS_MODE "\":\"%s\",\"" SS_TIME "\":%lld}"
                        " NX EX %llu",
                        sid.c_str(),static_cast<unsigned long long>(userId),mode.c_str(),
                        static_cast<long long>(GetNow()),static_cast<unsigned long long>(ttlSeconds)
                    );
                    if(result.type()==nosql::RedisResultType::kStatus && result.asString()==SS_OK){
                        // 说明成功执行
                        co_return sid;
                    }
            }catch(const nosql::RedisException& e){
                LOG_ERROR<<SS_LOG<<e.what();
            }
        }
        co_return "";
    }
    inline int64_t GetNow(){
        using namespace std::chrono;
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }
    Task<Json::Value> FindSession(const std::string& sid){
        // 如果没有对应的人或者出错了 就将userId设置为-1 顺便可以设置error字段
        auto redis=app().getRedisClient();
        Json::Value json;
        json[SS_UID]=-1;
        try{
            // nosql -> get ch:session:%s
            auto result=
                co_await redis->execCommandCoro("get " SS_KEY "%s",sid.c_str());
            if(result.type()==nosql::RedisResultType::kNil){
                json[SS_ERROR]=SS_NOEXIST;
            }else if(result.type()==nosql::RedisResultType::kString){
                // json=Json::parseFromStream(result.asString());
                Json::Reader reader;
                if(!reader.parse(result.asString(),json)){
                    // 解析失败
                    json[SS_UID]=-1;
                    json[SS_ERROR]=SS_UNKNOWN;
                }
            }
        }catch(const nosql::RedisException&e){
            json[SS_ERROR]=SS_REDISERR;
            LOG_ERROR<<SS_LOG<<e.what();
        }
        co_return json;
    }
    Task<bool> RevokeSession(const std::string& sid){
        auto redis=app().getRedisClient();
        bool res=true;
        try{
            // nosql del ch:session:%s 没有失败一说 只要没有catch都算成功
            co_await redis->execCommandCoro("del " SS_KEY "%s",sid.c_str());
        }catch(const nosql::RedisException&e){
            LOG_ERROR<<SS_LOG<<e.what();
            res=false;
        }
        co_return res;
    }
}
