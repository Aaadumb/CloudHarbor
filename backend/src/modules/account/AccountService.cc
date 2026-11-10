#include "AccountService.h"

#include <exception>
#include <regex>
#include <string_view>

#define AS_PWD_MIN "10"
#define AS_PWD_MAX "20"

namespace account::server{
    namespace {
        std::string trimName(const std::string& name) {
            const auto first = name.find_first_not_of(' ');
            if (first == std::string::npos) {
                return {};
            }
            const auto last = name.find_last_not_of(' ');
            return name.substr(first, last - first + 1);
        }

        [[maybe_unused]] bool checkName(const std::string& name) {
            const auto trimmed = trimName(name);
            if (trimmed.empty()) {
                return false;
            }

            for (std::size_t offset = 0, length = 0; offset < trimmed.size();) {
                if (++length > 50) {
                    return false;
                }

                const auto first = static_cast<unsigned char>(trimmed[offset]);
                if ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') ||
                    (first >= '0' && first <= '9') || first == ' ' || first == '_' || first == '-') {
                    ++offset;
                    continue;
                }

                // U+4E00–U+9FFF 中的汉字均编码为三个 UTF-8 字节。
                if (first < 0xE0 || first > 0xEF || trimmed.size() - offset < 3) {
                    return false;
                }
                const auto second = static_cast<unsigned char>(trimmed[offset + 1]);
                const auto third = static_cast<unsigned char>(trimmed[offset + 2]);
                if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) {
                    return false;
                }

                const auto codePoint = ((first & 0x0F) << 12) |
                                       ((second & 0x3F) << 6) | (third & 0x3F);
                if (codePoint < 0x4E00 || codePoint > 0x9FFF) {
                    return false;
                }
                offset += 3;
            }
            return true;
        }

        bool checkPassword(const std::string& password) {
            // 可打印 ASCII，从 ! 到 ~，不含空格。
            static const std::regex pattern("[!-~]{" AS_PWD_MIN "," AS_PWD_MAX "}");
            return std::regex_match(password, pattern);
        }

        class TransactionCommitAwaiter:public CallbackAwaiter<bool>{
        private:
            TransactionPtr& tx;
        public:
            TransactionCommitAwaiter(TransactionPtr& t):tx(t){};
            void await_suspend(std::coroutine_handle<> handle){
                auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
                tx->setCommitCallback([this,loop,handle](bool success){
                    loop->queueInLoop([this,handle,success]{
                        setValue(success);
                        handle.resume();
                    });
                });
                tx.reset();
            }
        };
    }

    Task<HttpResponsePtr> SendCode(const std::string &mail){
        // 这里 首先 判断是否处于冷却期 不处于冷却期 就发送验证码 并设置冷却期 并且绑定验证码
        // 先检查mail是否正确
        if(!mail::CheckMail(mail)){
            co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,"邮箱格式错误");
        }
        // 邮箱正确 检查是否处于冷却期
        bool cooling=co_await mail::IsMailCooling(mail);
        if(cooling){
            // 这里有两种情况 一种是服务器出错 一种是 真的处于冷却器 总是 都阻止操作
            co_return response::Failure(HttpStatusCode::k403Forbidden,errors::FORBIDDEN,"出错,请稍后尝试");
        }
        // 邮箱不处于冷却 先生成code
        std::string code;
        bool created=mail::CreateCode(code);
        if(!created){
            co_return response::InternalError();
        }
        // code创建成功 尝试绑定redis
        bool binded=co_await mail::BindRegisterCode(mail,code);
        if(!binded){
            co_return response::ServiceUnavailable();
        }
        // 绑定成功 尝试发送
        bool sent=co_await MailSender::getInstance().sendCode(mail,code);
        if(!sent){
            // 清理redis 然后co_return
            co_await mail::InvalidateRegisterCode(mail);
            co_return response::Failure(HttpStatusCode::k500InternalServerError,errors::SERVICE_UNAVAILABLE,"服务出错");
        }
        // 全部成功
        Json::Value resp;
        resp["status"]=true;
        co_return response::Success(resp);
    }


    Task<HttpResponsePtr> Login(const std::string& auth,const std::string& pwd,bool persistent){
        // 校验身份与密码 没问题 让nginx跳转 同时 根据persistent 设置 cookie
        // 把左右两端的空格去掉
        std::string trimmed=trimName(auth);
        Json::Value meta=co_await database::FindByAuth(trimmed);
        if(meta[UR_ID]==-1){
            if(meta[UR_ERROR]==UR_DB_ERROR){
                co_return response::ServiceUnavailable();
            }else{
                co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,"用户不存在");
            }
        }
        // 校验密码
        bool success=co_await PasswordHasher::getInstance().verifyPassword(pwd,meta[UR_PWD].asString());
        if(!success){
            co_return response::Failure(HttpStatusCode::k401Unauthorized,errors::AUTH_REQUIRED,"密码错误");
        }
        // 校验成功 根据persistent 确定cookie的maxage

        // 绑定cookie 持久cookie设置为一个月 不持久设置为30分钟
        uint64_t ttlsecond=persistent?2592000:1800;
        std::string mode=persistent?"持久":"临时";
        std::string cookie=co_await session::CreateSession(meta[UR_ID].asUInt64(),ttlsecond,mode);

        if(cookie.empty()){
            co_return response::ServiceUnavailable();
        }
        // 成功生成并绑定了cookie 现在让请求设置上cookie
        Json::Value res;
        res["status"]=true;
        HttpResponsePtr resp=response::Success(res);
        response::setLoginCookie(resp,cookie,persistent,ttlsecond);

        co_return resp;
    }


    Task<HttpResponsePtr> RegisterAccount(const std::string& name,const std::string& mail,
                                            const std::string& code,const std::string& pwd,bool persistent){
        // 检验一下邮箱和名称是否合理
        const std::string trimmedName=trimName(name);
        if(!checkName(trimmedName)||!mail::CheckMail(mail)){
            co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,"名称或邮箱不符合要求");
        }
        if(!checkPassword(pwd)){
            co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,
                "密码须为 " AS_PWD_MIN "～" AS_PWD_MAX " 个字符，只允许英文字母、数字和半角符号，不能包含空格");
        }
        // 校验一下验证码
        Json::Value res=co_await mail::ConsumeRegisterCode(mail,code);
        if(!res["valid"].asBool()){
            if(res["code"]=="UNKNOWN_ERROR"){
                co_return response::InternalError();
            }else{
                co_return response::Failure(HttpStatusCode::k400BadRequest,errors::VALIDATION_ERROR,"验证码失效或错误");
            }
        }
        // 验证码校验成功 本质上已经可以正常写入数据库了 现在将pwd hash
        std::string pwdhash;
        try{
            pwdhash=co_await PasswordHasher::getInstance().hashPassword(pwd);
        }catch(const std::exception& e){
            LOG_ERROR<<"注册密码哈希失败: "<<e.what();
            co_return response::InternalError();
        }catch(...){
            LOG_ERROR<<"注册密码哈希失败: 未知异常";
            co_return response::InternalError();
        }
        if(pwdhash.empty()){
            co_return response::InternalError();
        }
        // hash成功 先写MySQL 注册用于拿到userid 然后写redis
        TransactionPtr tx;
        try{
            tx=co_await app().getDbClient()->newTransactionCoro();
        }catch(const orm::DrogonDbException& e){
            LOG_ERROR<<"获取注册事务失败: "<<e.base().what();
            co_return response::ServiceUnavailable();
        }
        uint64_t userId=co_await database::CreateAccount(tx,trimmedName,mail,pwdhash);
        if(userId==-1){
            // 回滚
            tx->rollback();
            co_return response::Failure(HttpStatusCode::k503ServiceUnavailable,errors::SERVICE_UNAVAILABLE,"用户名或邮箱重名冲突，或服务出错");
        }
        // userid创建成功 现在创建root根目录并绑定
        uint64_t rootId=co_await file::database::CreateRoot(tx,userId);
        if(rootId==-1){
            tx->rollback();
            co_return response::ServiceUnavailable();
        }
        bool binded= co_await database::BindRoot(tx,userId,rootId);
        if(!binded){
            tx->rollback();
            co_return response::ServiceUnavailable();
        }
        // 已经完成了 现在提交事务
        bool success=co_await TransactionCommitAwaiter{tx};
        if(!success){
            co_return response::ServiceUnavailable();
        }
        // 用户已经创建成功 现在根据内容创建redis
        uint64_t ttlsecond=persistent?2592000:1800;
        std::string mode=persistent?"持久":"临时";
        std::string cookie=co_await session::CreateSession(userId,ttlsecond,mode);

        const bool loggedIn=!cookie.empty();
        Json::Value data;
        data["status"]=true;
        data["logged_in"]=loggedIn;
        HttpResponsePtr resp=response::Success(data,HttpStatusCode::k201Created,
            loggedIn?"账户创建成功":"注册成功，暂时无法自动登录，请稍后到登录页登录");
        if(loggedIn){
            response::setLoginCookie(resp,cookie,persistent,ttlsecond);
        }

        co_return resp;
    }


    Task<HttpResponsePtr> GetAccountInfo(uint64_t userId){

    }


    Task<HttpResponsePtr> GetAvatar(uint64_t userId){

    }
    
    
    Task<HttpResponsePtr> ModifyName(uint64_t userId,const std::string name){

    }


    Task<HttpResponsePtr> ModifyAvatar(uint64_t userId,const HttpFile& file){

    }


    Task<HttpResponsePtr> ModifyMail(uint64_t userId,const std::string oldmail,
                                        const std::string newmail,const std::string oldcode,
                                        const std::string newcode){

    }


    Task<HttpResponsePtr> ModifyPassword(uint64_t userId,const std::string& oldpwd,const std::string& newpwd){

    }


    Task<HttpResponsePtr> Logout(const std::string& sid){

    }

}
