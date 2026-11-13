#include "AccountService.h"

#include <exception>
#include <regex>
#include <string_view>

#define AS_PWD_MIN "10"
#define AS_PWD_MAX "20"

#define AS_MSG_BAD_MAIL "邮箱格式错误"
#define AS_MSG_NO_USER "用户不存在"
#define AS_MSG_BAD_CODE "验证码失效或错误"
#define AS_MSG_CONFLICT "重名冲突或服务出错"
#define AS_MSG_AUTH_FAILED "校验失败"

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
            co_return response::ValidationError(AS_MSG_BAD_MAIL);
        }
        // 邮箱正确 检查是否处于冷却期
        bool cooling=co_await mail::IsMailCooling(mail);
        if(cooling){
            // 这里有两种情况 一种是服务器出错 一种是 真的处于冷却器 总是 都阻止操作
            co_return response::Forbidden("出错,请稍后尝试");
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
        co_return response::Success();
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
                co_return response::ValidationError(AS_MSG_NO_USER);
            }
        }
        // 校验密码
        bool success=co_await PasswordHasher::getInstance().verifyPassword(pwd,meta[UR_PWD].asString());
        if(!success){
            co_return response::AuthRequired("密码错误");
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
        HttpResponsePtr resp=response::Success();
        response::setLoginCookie(resp,cookie,persistent,ttlsecond);

        co_return resp;
    }


    Task<HttpResponsePtr> RegisterAccount(const std::string& name,const std::string& mail,
                                            const std::string& code,const std::string& pwd,bool persistent){
        // 检验一下邮箱和名称是否合理
        const std::string trimmedName=trimName(name);
        if(!checkName(trimmedName)||!mail::CheckMail(mail)){
            co_return response::ValidationError("名称或邮箱不符合要求");
        }
        if(!checkPassword(pwd)){
            co_return response::ValidationError("密码须为 " AS_PWD_MIN "～" AS_PWD_MAX " 个字符，只允许英文字母、数字和半角符号，不能包含空格");
        }
        // 校验一下验证码
        Json::Value res=co_await mail::ConsumeRegisterCode(mail,code);
        if(!res["valid"].asBool()){
            if(res["code"]=="UNKNOWN_ERROR"){
                co_return response::InternalError();
            }else{
                co_return response::ValidationError(AS_MSG_BAD_CODE);
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
            co_return response::ServiceUnavailable("用户名或邮箱重名冲突，或服务出错");
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
        data[UR_STATUS]=true;
        data["loggedIn"]=loggedIn;
        HttpResponsePtr resp=response::Success(data,HttpStatusCode::k201Created,
            loggedIn?"账户创建成功":"注册成功，暂时无法自动登录，请稍后到登录页登录");
        if(loggedIn){
            response::setLoginCookie(resp,cookie,persistent,ttlsecond);
        }

        co_return resp;
    }


    Task<HttpResponsePtr> GetAccountInfo(uint64_t userId){
        // 得到用户的账户 name, mail,root_id avatar_path
        Json::Value meta=co_await database::GetAccountInfo(userId);
        if(meta[UR_ID]==-1){
            if(meta[UR_ERROR]==UR_NO_USER){
                co_return response::NotFound(AS_MSG_NO_USER);
            }else{
                co_return response::ServiceUnavailable();
            }
        }
        // 但是不返回给前端 应该由cookie来确认身份
        Json::Value data;
        data[UR_STATUS]=true;
        data[UR_NAME]=meta[UR_NAME];
        data[UR_MAIL]=meta[UR_MAIL];
        data["rootId"]=meta[UR_ROOT];
        data["avatarPath"]=meta[UR_AVATAR];
        co_return response::Success(data);
    }


    Task<HttpResponsePtr> GetAvatar(uint64_t userId){
        // 这个是要返回图片资源 需要通过nginx将图片转交给对方
        Json::Value meta=co_await database::GetAccountInfo(userId);
        if(meta[UR_ID]==-1){
            if(meta[UR_ERROR]==UR_NO_USER){
                co_return response::NotFound(AS_MSG_NO_USER);
            }
            else{
                co_return response::ServiceUnavailable();
            }
        }
        std::string path=meta[UR_AVATAR].asString();
        if(path=="default"){
            // 按理来说 前端html应该直接就有默认图像的内容 就不返回了
            co_return response::Forbidden();
        }
        // 首版统一返回png格式 如果原图是jpg格式 让浏览器自行识别
        HttpResponsePtr resp=response::Success();
        resp->addHeader("Content-Disposition","inline");
        response::SetStorageRedirect(resp,path);
        resp->addHeader("Content-Type","image/png");

        co_return resp;
    }
    
    
    Task<HttpResponsePtr> ModifyName(uint64_t userId,const std::string name){
        // 尝试修改 如果重名就返回失败即可
        if(!checkName(name)){
            co_return response::ValidationError("名称非法");
        }
        std::string trimmed=trimName(name);
        bool success=co_await database::ModifyName(userId,trimmed);
        if(!success){
            // 没成功 要么MySQL挂了 要么重名
            co_return response::ServiceUnavailable(AS_MSG_CONFLICT);
        }
        // 成功
        co_return response::Success();
    }


    Task<HttpResponsePtr> ModifyAvatar(uint64_t userId,const HttpFile& file){
        // 修改头像 需要先上传头像 然后将头像记录到MySQL 全部成功之后 返回给用户status
        // 先在临时目录保存
        // 先校验文件格式
        if(file.fileLength()==0){
            co_return response::ValidationError("文件错误");
        }
        if(file.getContentType()!=ContentType::CT_IMAGE_PNG && file.getContentType()!=ContentType::CT_IMAGE_JPG){
            co_return response::ValidationError("文件格式错误");
        }
        std::string path=co_await upload::ordinary::SaveOrdinaryFile(file);
        if(path.empty()){
            co_return response::InternalError();
        }
        // 通过fastdfs上传
        std::string storageId;
        bool uploaded=true;
        try{
            storageId=co_await FastDfsStorage::getInstance().uploadFile(path);
        }catch(const std::exception&e){
            LOG_ERROR<<"上传图片失败"<<e.what();
            uploaded=false;
        }catch(...){
            LOG_ERROR<<"上传图片失败:未知异常";
            uploaded=false;
        }
        try{
            if(!(co_await upload::ordinary::RemoveOrdinaryFile(path))){
                LOG_ERROR<<"event=temp_cleanup_failed operation=modify_avatar user_id="<<userId<<" path="<<path;
            }
        }catch(const std::exception& e){
            LOG_ERROR<<"event=temp_cleanup_failed operation=modify_avatar user_id="<<userId
                     <<" path="<<path<<" error="<<e.what();
        }
        if(!uploaded || storageId.empty()){
            co_return response::InternalError();
        }
        const auto result=co_await database::ModifyAvatar(userId,storageId);
        if(result==database::AvatarUpdateResult::Uncertain){
            LOG_ERROR<<"event=upload_mysql_result_uncertain operation=modify_avatar user_id="<<userId
                     <<" storage_id="<<storageId;
            co_return response::ServiceUnavailable();
        }
        if(result==database::AvatarUpdateResult::Rejected){
            try{
                if(!(co_await FastDfsStorage::getInstance().deleteFile(storageId))){
                    LOG_ERROR<<"event=fastdfs_delete_failed operation=modify_avatar user_id="<<userId
                             <<" storage_id="<<storageId;
                }
            }catch(const std::exception&e){
                LOG_ERROR<<"event=fastdfs_delete_failed operation=modify_avatar user_id="<<userId
                         <<" storage_id="<<storageId<<" error="<<e.what();
            }catch(...){
                LOG_ERROR<<"event=fastdfs_delete_failed operation=modify_avatar user_id="<<userId
                         <<" storage_id="<<storageId<<" error=unknown_exception";
            }
            co_return response::ServiceUnavailable();
        }
        co_return response::Success();
    }


    Task<HttpResponsePtr> ModifyMail(uint64_t userId,const std::string oldmail,
                                        const std::string newmail,const std::string oldcode,
                                        const std::string newcode){
        // 修改邮箱 验证一下 两个邮箱和验证码是否都匹配 并最终尝试写入数据库 看看会不会有冲突
        if(!mail::CheckMail(newmail)||!mail::CheckMail(oldmail)){
            co_return response::ValidationError(AS_MSG_BAD_MAIL);
        }
        if(newmail==oldmail){
            co_return response::ValidationError("新旧邮箱不能相同");
        }
        // 先检查旧邮箱
        Json::Value meta=co_await database::GetAccountInfo(userId);
        if(meta[UR_ID]==-1){
            if(meta[UR_ERROR]==UR_DB_ERROR){
                co_return response::ServiceUnavailable();
            }else{
                co_return response::AuthRequired(AS_MSG_AUTH_FAILED);
            }
        }
        if(meta[UR_MAIL].asString()!=oldmail){
            co_return response::ValidationError("错误操作");
        }
        // 匹配邮箱和验证码
        Json::Value res1=co_await mail::ConsumeRegisterCode(oldmail,oldcode);
        Json::Value res2=co_await mail::ConsumeRegisterCode(newmail,newcode);
        if(!res1["valid"].asBool()||!res2["valid"].asBool()){
            if(res1["code"]=="UNKNOWN_ERROR"||res2["code"]=="UNKNOWN_ERROR"){
                co_return response::ServiceUnavailable();
            }
            co_return response::ValidationError(AS_MSG_BAD_CODE);
        }
        // 匹配成功 尝试写入数据库
        bool success= co_await database::ModifyMail(userId,oldmail,newmail);
        if(!success){
            co_return response::ServiceUnavailable(AS_MSG_CONFLICT);
        }
        // 全部成功
        co_return response::Success();
    }


    Task<HttpResponsePtr> ModifyPassword(uint64_t userId,const std::string& oldpwd,const std::string& newpwd){
        // 匹配旧密码 hash新密码
        if(!checkPassword(oldpwd)||!checkPassword(newpwd)){
            co_return response::ValidationError("密码格式错误");
        }
        Json::Value meta=co_await database::GetAccountInfo(userId);
        if(meta[UR_ID]==-1){
            if(meta[UR_ERROR]==UR_DB_ERROR){
                co_return response::ServiceUnavailable();
            }
            co_return response::AuthRequired(AS_MSG_AUTH_FAILED);
        }
        std::string oldpwdhash=meta[UR_PWD].asString();
        // 校验旧密码
        bool success=co_await PasswordHasher::getInstance().verifyPassword(oldpwd,oldpwdhash);
        if(!success){
            co_return response::AuthRequired("服务出错或校验失败");
        }
        // 更换新密码
        std::string hash=co_await PasswordHasher::getInstance().hashPassword(newpwd);
        if(hash.empty()){
            co_return response::InternalError();
        }
        bool res=co_await database::ModifyPassword(userId,hash);
        if(!res){
            co_return response::ServiceUnavailable();
        }
        co_return response::Success();
    }


    Task<HttpResponsePtr> Logout(const std::string& sid){
        // 清理一下cookie
        if(!sid.empty() && !(co_await session::RevokeSession(sid))){
            co_return response::ServiceUnavailable();
        }
        HttpResponsePtr resp=response::Success();
        response::clearLoginCookie(resp);
        co_return resp;
    }

}
