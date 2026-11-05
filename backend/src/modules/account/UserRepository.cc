#include "UserRepository.h"
namespace account{
    namespace database{
        Task<Json::Value> FindByAuth(const std::string& auth){
            auto db=app().getDbClient();
            std::string field=UR_NAME;
            if(account::mail::CheckMail(auth)){
                field=UR_MAIL;
            }
            Json::Value json;
            json[UR_ID]=-1;
            try{
                // sql -> select id,name,password,mail from user where ?=?
                auto result=
                    co_await db->execSqlCoro("select id,password from user where "+field+"=?",auth);
                if(result.empty()){
                    json[UR_ERROR]=UR_NO_USER;
                }else{
                    auto res=result[0];
                    json[UR_ID]=res[UR_ID].as<uint64_t>();
                    json[UR_PWD]=res[UR_PWD].as<std::string>();
                }
            }catch(const DrogonDbException& e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
                json[UR_ERROR]=UR_DB_ERROR;
            }
            co_return json;
        }
        Task<Json::Value> GetAccountInfo(uint64_t userId){
            // 返回给前端 id root_id avatar_path name mail
            auto db=app().getDbClient();
            Json::Value json;
            json[UR_ID]=-1;
            try{
                // sql -> select id,name,mail,root_id,avatar_path from user where id=?
                auto result=
                    co_await db->execSqlCoro(
                        "select name,mail,root_id,avatar_path from user where id=?",
                        userId);
                if(result.empty()){
                    json[UR_ERROR]=UR_NO_USER;
                }else{
                    auto res=result[0];
                    json[UR_ID]=userId;
                    json[UR_NAME]=res[UR_NAME].as<std::string>();
                    json[UR_MAIL]=res[UR_MAIL].as<std::string>();
                    json[UR_ROOT]=res[UR_ROOT].as<uint64_t>();
                    if(res[UR_AVATAR].isNull()){
                        json[UR_AVATAR]="default";
                    }else{
                        json[UR_AVATAR]=res[UR_AVATAR].as<std::string>();
                    }
                }
            }catch(const DrogonDbException& e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
                json[UR_ERROR]=UR_DB_ERROR;
            }
            co_return json;
        }
        Task<bool> IsNameUsed(const std::string& name){
            auto db=app().getDbClient();
            bool ans=true;
            try{
                // sql -> select count(*) from user where name=?
                auto result=
                    co_await db->execSqlCoro("select count(*) as status from user where name = ?",name);
                ans=result[0][UR_STATUS].as<int>();
            }catch(const DrogonDbException&e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
            }
            co_return ans;
        }
        Task<bool> IsMailUsed(const std::string &mail){
            auto db=app().getDbClient();
            bool ans=true;
            try{
                // sql -> select count(*) from user where mail=?
                auto result=
                    co_await db->execSqlCoro("select count(*) as status from user where mail = ?",mail);
                ans=result[0][UR_STATUS].as<int>();
            }catch(const DrogonDbException&e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
            }
            co_return ans;
        }
        Task<uint64_t> CreateAccount(const TransactionPtr&tx,
                                    const std::string&name,
                                    const std::string&mail,
                                    const std::string&passwordHash){
            // 不commit commit由上层管理 根据结果而定
            // 出错也不提前回滚 而是返回-1 让上层回滚 统一出入口
            uint64_t ans=-1;
            try{
                // sql -> insert into user values(null,name,password,mail);
                auto result=
                    co_await tx->execSqlCoro(
                        "insert into user(name,password,mail,root_id,avatar_path)\
                        values(?,?,?,NULL,NULL)",
                        name,passwordHash,mail);
                // 插入失败直接报错 走到这里就是成功
                ans=result.insertId();
            }catch(const DrogonDbException&e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
            }
            co_return ans;
        }
        Task<bool> BindRoot(const TransactionPtr& tx,uint64_t userId,uint64_t rootId){
            bool ans=false;
            try{
                // sql update user set root_id= ? where id=?
                co_await tx->execSqlCoro("update user set root_id=? where id=?",rootId,userId);
                // 执行到这里就是成功更新
                ans=true;
            }catch(const DrogonDbException& e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
            }
            co_return ans;
        }
        Task<bool> ModifyFiled(uint64_t userId,const std::string filed,const std::string& content){
            auto db=app().getDbClient();
            bool res=false;
            try{
                co_await db->execSqlCoro("update user set "+filed+"=? where id=?",content,userId);
                res=true;
            }catch(const DrogonDbException& e){
                LOG_ERROR<<UR_DB_LOG<<e.base().what();
            }
            co_return res;
        }
        Task<bool> ModifyName(uint64_t userId,const std::string& name){
            co_return co_await ModifyFiled(userId,UR_NAME,name);
        }
        Task<bool> ModifyMail(uint64_t userId,const std::string&mail){
            co_return co_await ModifyFiled(userId,UR_MAIL,mail);
        }
        Task<bool> ModifyPassword(uint64_t userId,const std::string& pwdhash){
            co_return co_await ModifyFiled(userId,UR_PWD,pwdhash);
        }
        Task<bool> ModifyAvatar(uint64_t userId,const std::string&path){
            co_return co_await ModifyFiled(userId,UR_AVATAR,path);
        }
    }
}
