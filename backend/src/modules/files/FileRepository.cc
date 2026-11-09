#include "FileRepository.h"
#include "cloudharbor/common/AppError.h"
#include <unordered_map>
#include <utility>

namespace file::database{
    namespace{
        constexpr int MYSQL_DUPLICATE_ENTRY=1062;

        const char* CreateEntryErrorCode(const DrogonDbException& e){
            const auto* sqlError=dynamic_cast<const SqlError*>(&e.base());
            if(sqlError && sqlError->errcode()==MYSQL_DUPLICATE_ENTRY){
                return errors::NAME_CONFLICT;
            }
            return FR_DB_ERROR;
        }

        Json::Value ReadChildEntry(const Row& row){
            Json::Value entry;
            entry[FR_ID]=row[FR_ID].as<uint64_t>();
            entry[FR_NAME]=row[FR_NAME].as<std::string>();
            entry[FR_TYPE]=row[FR_TYPE].as<std::string>();

            if(row[FR_SIZE].isNull())entry[FR_SIZE]=-1;
            else entry[FR_SIZE]=row[FR_SIZE].as<uint64_t>();

            if(row[FR_LAST_MODIFY].isNull())entry[FR_LAST_MODIFY]=Json::nullValue;
            else entry[FR_LAST_MODIFY]=row[FR_LAST_MODIFY].as<std::string>();

            if(row[FR_PATH].isNull())entry[FR_PATH]=Json::nullValue;
            else entry[FR_PATH]=row[FR_PATH].as<std::string>();
            return entry;
        }
    }

    Task<Json::Value> GetEntry(uint64_t userId,uint64_t entryId){
        // 返回大部分字段
        // 几种出错情况下的返回
        // 出错/不符合要求 id字段统一返回-1 同时error设置错误原因
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_ID]=-1;
        try{
            // sql select id,name,type,father,size,last_modify,path from file where id=? and owner=?
            auto result=
                co_await db->execSqlCoro(
                    "select id,name,type,father,size,last_modify,path "
                    "from file where id=? and owner=?",
                    entryId,userId);
            // 未正确查询到
            if(result.empty()){
                // 第一种 目录不存在 第二种 用户不存在 第三种 用户和目录不匹配
                json[FR_ERROR]=FR_NO_ENTRY_OR_MISMATCH;
            }else{
                // 查询到结果
                auto res=result[0];
                json[FR_ID]=res[FR_ID].as<uint64_t>();
                json[FR_NAME]=res[FR_NAME].as<std::string>();
                json[FR_TYPE]=res[FR_TYPE].as<std::string>();
                // 数字字段用 -1 表示 SQL NULL，字符串字段用 JSON null。
                if(res[FR_FATHER].isNull())json[FR_FATHER]=-1;
                else json[FR_FATHER]=res[FR_FATHER].as<uint64_t>();

                if(res[FR_SIZE].isNull())json[FR_SIZE]=-1;
                else json[FR_SIZE]=res[FR_SIZE].as<uint64_t>();

                if(res[FR_LAST_MODIFY].isNull())json[FR_LAST_MODIFY]=Json::nullValue;
                else json[FR_LAST_MODIFY]=res[FR_LAST_MODIFY].as<std::string>();

                if(res[FR_PATH].isNull())json[FR_PATH]=Json::nullValue;
                else json[FR_PATH]=res[FR_PATH].as<std::string>();
            }

        }catch(const DrogonDbException& e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
    Task<Json::Value> ListChildren(uint64_t userId,uint64_t parentId){
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_STATUS]=false;
        try{
            // sql -> select id,name,type,size,last_modify,path from file where father=? and owner=?
            auto result=
                co_await db->execSqlCoro(
                    "select id,name,type,size,last_modify,path "
                    "from file where father=? and owner=?",
                    parentId,userId);
                // 即使为空 执行到这里也算成功 server层应该提前检查 userId和parentId是否合理
                json[FR_STATUS]=true;
                json[FR_DATA]=Json::arrayValue;
                for(const auto& row:result){
                    json[FR_DATA].append(ReadChildEntry(row));
                }
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
    Task<Json::Value> ListChildren(uint64_t userId,const std::string& path){
        Json::Value json;
        json[FR_STATUS]=false;
        if(path.empty() || path.front()!='/' || (path.size()>1 && path.back()=='/')){
            json[FR_ERROR]=FR_NO_ENTRY;
            co_return json;
        }

        // file.name 最多 255 个字符，utf8mb4 每个字符最多 4 个字节。
        constexpr std::size_t maxSegmentBytes=255*4;
        Json::Value segments(Json::arrayValue);
        for(std::size_t start=1;start<path.size();){
            auto end=path.find('/',start);
            if(end==std::string::npos)end=path.size();
            const auto segment=path.substr(start,end-start);
            if(segment.empty() || segment=="." || segment==".." ||
               segment.size()>maxSegmentBytes || segment.find('\0')!=std::string::npos){
                json[FR_ERROR]=FR_NO_ENTRY;
                co_return json;
            }
            segments.append(segment);
            start=end+1;
        }

        Json::StreamWriterBuilder writer;
        writer["indentation"]="";
        const auto segmentsJson=Json::writeString(writer,segments);

        try{
            auto result=co_await app().getDbClient()->execSqlCoro(
                R"sql(
                    WITH RECURSIVE
                    segments AS (
                        SELECT s.pos,s.segment
                        FROM JSON_TABLE(?, '$[*]' COLUMNS (
                            pos FOR ORDINALITY,
                            segment VARCHAR(1020) CHARACTER SET utf8mb4 PATH '$'
                        )) AS s
                    ),
                    walk (id,owner,depth) AS (
                        SELECT f.id,f.owner,0
                        FROM `user` AS u
                        JOIN `file` AS f ON f.id=u.root_id AND f.owner=u.id
                        WHERE u.id=? AND f.type='folder' AND f.father IS NULL

                        UNION ALL

                        SELECT f.id,f.owner,s.pos
                        FROM walk AS w
                        JOIN segments AS s ON s.pos=w.depth+1
                        JOIN `file` AS f ON f.father=w.id AND f.owner=w.owner
                                        AND f.type='folder'
                                        AND f.name=s.segment COLLATE utf8mb4_0900_bin
                    ),
                    target_dir AS (
                        SELECT id,owner FROM walk
                        WHERE depth=(SELECT COUNT(*) FROM segments)
                    )
                    SELECT child.id,child.name,child.type,child.size,child.last_modify,child.path
                    FROM target_dir AS d
                    LEFT JOIN `file` AS child ON child.father=d.id AND child.owner=d.owner
                    ORDER BY (child.type='folder') DESC,child.name,child.id
                )sql",
                segmentsJson,userId);

            if(result.empty()){
                json[FR_ERROR]=FR_NO_ENTRY;
                co_return json;
            }
            json[FR_STATUS]=true;
            json[FR_DATA]=Json::arrayValue;
            for(const auto& row:result){
                // LEFT JOIN 在空目录时保留一行，子项字段均为 NULL。
                if(!row[FR_ID].isNull())json[FR_DATA].append(ReadChildEntry(row));
            }
        }catch(const DrogonDbException& e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
    Task<Json::Value> FindChildEntry(uint64_t userId,uint64_t parentId,const std::string&name){
        // 主要是查找目录下是否有某一名称的目录/文件 提前防止创建同名目录/文件 这一点跟设计初有别
        // 因为MySQL设置的是name,father,owner的联合unique键 没有加type 而且文件夹和文件不重名 在设计上也说的过去
        // 成功就返回对应的id 失败就设置error 不再返回多余字段
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_ID]=-1;
        try{
            // sql select id from file where father=? and owner=? and name=?
            auto result=
                co_await db->execSqlCoro(
                    "select id from file "
                    "where father=? and owner=? and name=?",
                    parentId,userId,name);
            if(result.empty()){
                json[FR_ERROR]=FR_NO_ENTRY;
            }else{
                json[FR_ID]=result[0][FR_ID].as<uint64_t>();
            }
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
    Task<Json::Value> FindFileByStorageId(uint64_t userId,const std::string& storageId){
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_ID]=-1;
        try{
            const auto result=co_await db->execSqlCoro(
                "select id from file where owner=? and type=? "
                "and cast(path as binary)=cast(? as binary) limit 1",
                userId,FR_FILE,storageId);
            if(result.empty()){
                json[FR_ERROR]=FR_NO_ENTRY;
            }else{
                json[FR_ID]=result[0][FR_ID].as<uint64_t>();
            }
        }catch(const DrogonDbException& e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }

    Task<uint64_t> CreateRoot(const TransactionPtr&tx,uint64_t userId){
        // 成功返回inserId 失败返回-1 事务回滚由上层管理 注意这个-1实际上是UINT64_MAX
        uint64_t res=-1;
        try{
            // sql-> insert into file(name,type,owner) values(?,?,?)
            auto result=
                co_await tx->execSqlCoro(
                    "insert into file(name,type,owner) values(?,?,?)",
                    "/",FR_FOLDER,userId);
            // 执行到这里说明执行成功
            res=result.insertId();
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
        }
        co_return res;
    }
    Task<Json::Value> CreateDirectory(uint64_t userId,Json::Value metaData){
        // metaData 提供 name,father两个字段即可
        // 返回id,错误时id=-1&&error表示错误原因
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_ID]=-1;
        try{
            // sql insert into file(name,father,owner,type) values(?,?,?,?)
            auto result=
                co_await db->execSqlCoro(
                    "insert into file(name,father,owner,type) values(?,?,?,?)",
                    metaData[FR_NAME],metaData[FR_FATHER],userId,FR_FOLDER);
            json[FR_ID]=static_cast<uint64_t>(result.insertId());
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=CreateEntryErrorCode(e);
        }
        co_return json;
    }
    Task<Json::Value> CreateFile(uint64_t userId,Json::Value metaData){
        // medaData 提供 name,father,size,last_modify,path
        // 返回id字段 错误就返回-1跟error
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_ID]=-1;
        try{
            // sql insert into file(name,type,owner,father,size,last_modify,path) value(?,?,?,?,?,?,?)
            auto result=
                co_await db->execSqlCoro(
                    "insert into file"
                    "(name,type,owner,father,size,last_modify,path) "
                    "values(?,?,?,?,?,?,?)",
                    metaData[FR_NAME],FR_FILE,userId,metaData[FR_FATHER],
                    metaData[FR_SIZE],metaData[FR_LAST_MODIFY],metaData[FR_PATH]);
            json[FR_ID]=static_cast<uint64_t>(result.insertId());
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=CreateEntryErrorCode(e);
        }
        co_return json;
    }
    Task<Json::Value> DeleteEntries(const TransactionPtr&tx,uint64_t userId,const std::vector<uint64_t>& entryIds){
        // 梳理一下整体思路 先查找出所有的记录 然后执行删除操作 并返回所有的物理文件的id和path
        Json::Value json;
        json[FR_STATUS]=false;
        if(entryIds.empty()){
            json[FR_STATUS]=true;
            json[FR_DATA]=Json::arrayValue;
            co_return json;
        }
        std::vector<uint64_t> params;

        auto placeholders=[](std::size_t count){
            std::string str="?";
            for(std::size_t i=1;i<count;i++){
                str+=",?";
            }
            return str;
        };

        params.push_back(userId);
        params.insert(params.end(),entryIds.begin(),entryIds.end());
        params.push_back(userId);
        try{
            auto result=
                co_await tx->execSqlCoro(
                    "with recursive targets (id,type) as( "
                    "select id,type from file "
                    "where owner=? "
                    "and id in ("+placeholders(entryIds.size())+
                    ") and father is not null "
                    "union distinct "
                    "select child.id,child.type "
                    "from targets as parent "
                    "join file as child on child.father=parent.id "
                    "where parent.type='folder' "
                    "and child.owner=?) "
                    "select f.id,f.father,f.type,f.path "
                    "from targets as t "
                    "join file as f on f.id=t.id;",
                    std::as_const(params)
                );
            if(result.empty()){
                json[FR_STATUS]=true;
                json[FR_DATA]=Json::arrayValue;
                co_return json;
            }

            struct Target{
                uint64_t father;
                std::size_t remainingChildren=0;
            };
            std::unordered_map<uint64_t,Target> targets;
            Json::Value cleanup(Json::arrayValue);
            for(const auto& row:result){
                const auto id=row[FR_ID].as<uint64_t>();
                targets.emplace(id,Target{row[FR_FATHER].as<uint64_t>()});
                if(row[FR_TYPE].as<std::string>()==FR_FILE){
                    Json::Value item;
                    item[FR_ID]=id;
                    item[FR_PATH]=row[FR_PATH].as<std::string>();
                    cleanup.append(std::move(item));
                }
            }

            for(const auto& entry:targets){
                if(auto parent=targets.find(entry.second.father);parent!=targets.end()){
                    ++parent->second.remainingChildren;
                }
            }
            std::vector<uint64_t> orderedIds;
            orderedIds.reserve(targets.size());
            for(const auto& [id,target]:targets){
                if(target.remainingChildren==0){
                    orderedIds.push_back(id);
                }
            }
            // 待删子项全部排在父目录之前。
            for(std::size_t i=0;i<orderedIds.size();i++){
                const auto father=targets.at(orderedIds[i]).father;
                if(auto parent=targets.find(father);parent!=targets.end()){
                    if(--parent->second.remainingChildren==0){
                        orderedIds.push_back(father);
                    }
                }
            }
            if(orderedIds.size()!=targets.size()){
                LOG_ERROR<<"Cycle in file deletion targets, userId="<<userId;
                json[FR_ERROR]=FR_DB_ERROR;
                co_return json;
            }

            const auto slots=placeholders(orderedIds.size());
            std::vector<uint64_t> deleteParams{userId};
            deleteParams.reserve(1+2*orderedIds.size());
            deleteParams.insert(deleteParams.end(),orderedIds.begin(),orderedIds.end());
            deleteParams.insert(deleteParams.end(),orderedIds.begin(),orderedIds.end());
            auto resultDel=
                co_await tx->execSqlCoro(
                    "delete from file where owner=? and id in ("+slots+
                    ") order by field(id,"+slots+");",
                    std::as_const(deleteParams)
                );
            if(resultDel.affectedRows()!=orderedIds.size()){
                LOG_ERROR<<"Unexpected deleted row count, userId="<<userId;
                json[FR_ERROR]=FR_DB_ERROR;
                co_return json;
            }
            json[FR_STATUS]=true;
            json[FR_DATA]=std::move(cleanup);
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
    Task<Json::Value> GetEntries(uint64_t userId,const std::vector<uint64_t>&entrIds){
        auto db=app().getDbClient();
        Json::Value json;
        json[FR_STATUS]=false;
        std::vector<uint64_t> params(entrIds);
        params.push_back(userId);
        auto placeholders=[](std::size_t count){
            std::string str="?";
            for(std::size_t i=1;i<count;i++){
                str+=",?";
            }
            return str;
        };
        try{
            // sql -> select id,name,type,father,size,last_modify,path from file where id in (?,?,?) and owner=?
            auto result=co_await db->execSqlCoro(
                "select id,name,type,father,size,last_modify,path "
                "from file where id in ("+placeholders(entrIds.size())+") "
                "and owner=?",
                std::as_const(params)
            );
            json[FR_STATUS]=true;
            json[FR_DATA]=Json::arrayValue;
            for(auto res:result){
                Json::Value tmp;
                tmp[FR_ID]=res[FR_ID].as<uint64_t>();
                tmp[FR_NAME]=res[FR_NAME].as<std::string>();
                tmp[FR_TYPE]=res[FR_TYPE].as<std::string>();
                // 数字字段用 -1 表示 SQL NULL，字符串字段用 JSON null。
                if(res[FR_FATHER].isNull())tmp[FR_FATHER]=-1;
                else tmp[FR_FATHER]=res[FR_FATHER].as<uint64_t>();

                if(res[FR_SIZE].isNull())tmp[FR_SIZE]=-1;
                else tmp[FR_SIZE]=res[FR_SIZE].as<uint64_t>();

                if(res[FR_LAST_MODIFY].isNull())tmp[FR_LAST_MODIFY]=Json::nullValue;
                else tmp[FR_LAST_MODIFY]=res[FR_LAST_MODIFY].as<std::string>();

                if(res[FR_PATH].isNull())tmp[FR_PATH]=Json::nullValue;
                else tmp[FR_PATH]=res[FR_PATH].as<std::string>();

                json[FR_DATA].append(tmp);
            }
        }catch(const DrogonDbException&e){
            LOG_ERROR<<FR_DB_LOG<<e.base().what();
            json[FR_ERROR]=FR_DB_ERROR;
        }
        co_return json;
    }
}
