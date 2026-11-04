#include "VerificationCodeStore.h"
namespace account::mail{
    bool CreateCode(std::string &out,int len){
        out.clear();
        while(out.size()<len){
            unsigned char tmp;
            ssize_t res=getrandom(&tmp,sizeof tmp,0);
            if(res<0){
                if(errno==EINTR)continue;
                out.clear();
                return false;
            }
            if(tmp<250){
                // 大于等于250拒绝采样 结果更加随机
                out+=static_cast<char>('0'+tmp%10);
            }
        }
        return true;
    }
    bool CheckMail(const std::string& mail){
        if(mail.empty() || mail.length()>254)return false;
        static const std::regex re(
            R"(^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]{1,64}@)"
            R"([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)"
            R"((\.[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)+$)");
        if (!std::regex_match(mail, re)) return false;
        return true;
    }
    Task<bool> BindRegisterCode(const std::string&mail,const std::string& code){
        std::string codeKey=CH_REGISTER_CODE_KEY_PREFIX+mail;
        std::string coolKey=CH_REGISTER_COOLDOWN_KEY_PREFIX+mail;
        bool success=false;
        auto redis=app().getRedisClient();
        try{
            // 先绑定验证码 然后绑定邮箱发送冷却 两者合并为一个事务
            // 开启事务
            auto trans=co_await redis->newTransactionCoro();

            // 入队命令
            // redis没有回滚机制 执行事务 中途有个别执行失败 整体返回false 然后清理key
            co_await trans->execCommandCoro("hmset %s " CH_VERIFICATION_CODE_FIELD " %s " CH_VERIFICATION_ATTEMPTS_FIELD " %d",codeKey.c_str(),code.c_str(),5);
            co_await trans->execCommandCoro("expire %s %d",codeKey.c_str(),5*60);
            co_await trans->execCommandCoro("set %s 1 NX EX %d",coolKey.c_str(),60);

            // 提交并执行
            auto result=co_await trans->executeCoro();
            if(result.type()==nosql::RedisResultType::kArray){
                auto replies=result.asArray();
                // 验证码写入、有效期和冷却记录都成功才算绑定完成
                success=replies.size()==3
                    && replies[0].type()==nosql::RedisResultType::kStatus
                    && replies[0].asString()==CH_VERIFICATION_OK
                    && replies[1].type()==nosql::RedisResultType::kInteger
                    && replies[1].asInteger()==1
                    && replies[2].type()==nosql::RedisResultType::kStatus
                    && replies[2].asString()==CH_VERIFICATION_OK;
            }
            if(!success){
                LOG_ERROR << "Unexpected transaction result when binding registration code";
            }
        }catch(const nosql::RedisException& e){
            success=false;
            LOG_ERROR << "Failed to bind registration code: " << e.what();
        }
        // 执行完try catch后 根据success的值 确定是否清理key 以及确认返回值
        if(!success){
            // 清理失败也返回false，但不能据此认为两个key已清除
            co_await InvalidateRegisterCode(mail);
        }
        co_return success;
    }
    Task<bool> InvalidateRegisterCode(const std::string&mail){
        auto redis=app().getRedisClient();
        std::string codeKey=CH_REGISTER_CODE_KEY_PREFIX+mail;
        std::string coolKey=CH_REGISTER_COOLDOWN_KEY_PREFIX+mail;
        bool res=true;
        try{
            co_await redis->execCommandCoro("del %s %s",codeKey.c_str(),coolKey.c_str());
        }catch(const nosql::RedisException& e){
            res=false;
            LOG_ERROR<<"Fialed to clean up registration code: "<<e.what();
        }
        co_return res;
    }
    Task<bool> ExistMail(const std::string& mail){
        // 检查是否存在某个邮箱的key
        std::string key=CH_REGISTER_CODE_KEY_PREFIX+mail;
        bool res=true;
        auto redis=app().getRedisClient();
        try{
            auto result=co_await redis->execCommandCoro(CH_REDIS_EXISTS_COMMAND,key.c_str());
            if(result.type()!=nosql::RedisResultType::kInteger){
                res=false;
            }else{
                res=result.asInteger();
            }
        }catch(const nosql::RedisException& e){
            // res也设置为false 服务端错误导致的无法查验 理应阻止下一步进行
            res=false;
            LOG_ERROR<<CH_REDIS_ERROR_PREFIX " "<<e.what();
        }
        co_return res;
    }
    Task<bool> IsMailCooling(const std::string&mail){
        std::string key=CH_REGISTER_COOLDOWN_KEY_PREFIX+mail;
        auto redis=app().getRedisClient();
        bool res=true;
        try{
            auto result=co_await redis->execCommandCoro(CH_REDIS_EXISTS_COMMAND,key.c_str());
            if(result.type()!=nosql::RedisResultType::kInteger){
                // 无法判断就判定的情况 应该阻止未知操作 讲结果设置为true;
                res=true;
            }else{
                res=result.asInteger();
            }
        }catch(const nosql::RedisException&e){
            res=true;
            LOG_ERROR<<CH_REDIS_ERROR_PREFIX " "<<e.what();
        }
        co_return res;
    }
    Task<Json::Value> ConsumeRegisterCode(const std::string& mail,const std::string& code){
        // 原子完成比较、错误扣次和正确消费，只操作验证码key，保留冷却记录
        // 脚本返回：-1 不存在或过期，0 验证失败，1 验证成功并已消费
        static constexpr char consumeScript[]=R"lua(
            local values = redis.call('HMGET', KEYS[1], ARGV[2], ARGV[3])
            if not values[1] then
                return -1
            end

            local remaining = tonumber(values[2])
            if not remaining or remaining ~= math.floor(remaining) then
                return redis.error_reply('Invalid registration code attempt count')
            end
            if remaining <= 0 then
                redis.call('DEL', KEYS[1])
                return 0
            end

            if values[1] == ARGV[1] then
                redis.call('DEL', KEYS[1])
                return 1
            end

            remaining = redis.call('HINCRBY', KEYS[1], ARGV[3], -1)
            if remaining <= 0 then
                redis.call('DEL', KEYS[1])
            end
            return 0
        )lua";
        std::string key=CH_REGISTER_CODE_KEY_PREFIX+mail;
        auto redis=app().getRedisClient();
        Json::Value json;
        json[CH_VERIFICATION_VALID_FIELD]=false;
        json[CH_VERIFICATION_CODE_FIELD]=CH_VERIFICATION_UNKNOWN_ERROR;
        try{
            auto result=co_await redis->execCommandCoro(
                "eval %s 1 %s %s %s %s",consumeScript,key.c_str(),code.c_str(),
                CH_VERIFICATION_CODE_FIELD,CH_VERIFICATION_ATTEMPTS_FIELD);
            if(result.type()==nosql::RedisResultType::kInteger){
                auto status=result.asInteger();
                if(status==1){
                    json[CH_VERIFICATION_VALID_FIELD]=true;
                    json[CH_VERIFICATION_CODE_FIELD]=CH_VERIFICATION_OK;
                }else if(status==0){
                    json[CH_VERIFICATION_CODE_FIELD]=CH_VERIFICATION_INVALID;
                }else if(status==-1){
                    json[CH_VERIFICATION_CODE_FIELD]=CH_VERIFICATION_EXPIRED;
                }else{
                    LOG_ERROR << "Unexpected registration code validation status";
                }
            }else{
                LOG_ERROR << "Unexpected registration code validation result type";
            }
        }catch(const nosql::RedisException& e){
            // Redis故障与验证码业务失败通过不同的code区分
            LOG_ERROR<<CH_REDIS_ERROR_PREFIX " "<<e.what();
        }
        co_return json;
    }
}
