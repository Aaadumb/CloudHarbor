#include "UploadTempStorage.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>

namespace {
    bool IsUploadId(const std::string& uploadId){
        return uploadId.size()==32 &&
            uploadId.find_first_not_of("0123456789abcdef")==std::string::npos;
    }

    std::string TaskPath(const std::string& uploadId){
        return IsUploadId(uploadId) ? UTS_PATH_ROOT+uploadId : std::string{};
    }

    constexpr size_t kCopyBufferSize=64*1024;

    bool CopyChunk(const fs::path& path,std::ofstream& output,
                std::array<char,kCopyBufferSize>& buffer){
        std::ifstream input(path,std::ios::binary);
        if(!input)return false;
        while(input.read(buffer.data(),buffer.size()) || input.gcount()>0){
            if(!output.write(buffer.data(),input.gcount()))return false;
        }
        return input.eof() && !input.bad();
    }
}

CreateTaskDirectoryAwaiter::CreateTaskDirectoryAwaiter(const std::string&uploadId):
    path(TaskPath(uploadId)){};


bool CreateTaskDirectoryAwaiter::createTaskDirectory(){
    if(path.empty())return false;
    std::error_code ec;
    fs::create_directories(UTS_PATH_ROOT,ec);
    if(ec){
        LOG_ERROR<<"create directories error:"<<ec.message();
        return false;
    }
    // 只有实际创建了任务目录，才允许继续初始化或回收它。
    if(!fs::create_directory(path,ec)){
        if(ec)LOG_ERROR<<"create task directory error:"<<ec.message();
        return false;
    }
    if(fs::create_directory(path+"/chunks",ec))return true;
    if(ec)LOG_ERROR<<"create chunks directory error:"<<ec.message();
    fs::remove(path,ec);
    if(ec)LOG_ERROR<<"remove empty task directory error:"<<ec.message();
    return false;
}
void CreateTaskDirectoryAwaiter::await_suspend(std::coroutine_handle<> handle){
    // 把任务交给线程池异步完成即可
    auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop]{
        // 执行函数操作
        std::exception_ptr error;
        bool res=false;
        try{
            res=createTaskDirectory();
        }catch(...){
            error=std::current_exception();
        }
        
        loop->queueInLoop([this,error,res,handle]{
            if(error){
                setException(error);
            }else{
                setValue(res);
            }
            handle.resume();
        });
    });
    if(!success){
        setValue(false);
        handle.resume();
    }
}
SaveChunkAwaiter::SaveChunkAwaiter(const std::string& uploadId,uint64_t idx,const HttpFile&file):
    path(IsUploadId(uploadId) ? UTS_PATH_ROOT+uploadId+"/chunks/" : std::string{}),
    index(idx),chunk(file){};

bool SaveChunkAwaiter::saveChunk(){
    std::error_code ec;
    if(!fs::is_directory(path,ec))return false;
    const auto target=fs::path(path)/(std::to_string(index)+".part");
    const auto temporary=target.string()+".tmp";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    if(!output)return false;

    const auto content=chunk.fileContent();
    output.write(content.data(),content.size());
    output.close();
    if(output){
        // 同编号可以重写，写失败时保留原分片。
        fs::rename(temporary,target,ec);
        if(!ec)return true;
    }
    fs::remove(temporary,ec);
    if(ec)LOG_ERROR<<"remove incomplete chunk error:"<<ec.message();
    return false;
}

void SaveChunkAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,loop,handle]{
        std::exception_ptr error;
        bool res=false;
        try{
            res=saveChunk();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,error,res,handle]{
            if(error){
                setException(error);
            }else{
                setValue(res);
            }
            handle.resume();
        });
    });
    if(!success){
        setValue(false);
        handle.resume();
    }
}

MergeChunksAwaiter::MergeChunksAwaiter(const std::string& uploadId,uint64_t total,uint64_t size):
    path(TaskPath(uploadId)),totalChunkes(total),expectedSize(size){};

std::string MergeChunksAwaiter::mergeChunks(){
    if(path.empty() || totalChunkes==0)return {};
    const fs::path taskDirectory(path);
    const auto chunks=taskDirectory/"chunks";
    std::error_code ec;
    if(!fs::is_directory(chunks,ec))return {};

    const auto temporary=taskDirectory/"merged.tmp";
    const auto target=taskDirectory/"merged.bin";
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
    if(!output)return {};

    std::array<char,kCopyBufferSize> buffer;
    bool copied=true;
    for(uint64_t i=0;i<totalChunkes;i++){
        if(!CopyChunk(chunks/(std::to_string(i+1)+".part"),output,buffer)){
            copied=false;
            break;
        }
    }
    output.close();
    if(copied && output && fs::file_size(temporary,ec)==expectedSize && !ec){
        fs::rename(temporary,target,ec);
        if(!ec)return target.string();
    }
    fs::remove(temporary,ec);
    if(ec)LOG_ERROR<<"remove incomplete merge error:"<<ec.message();
    return {};
}

void MergeChunksAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,loop,handle]{
        std::exception_ptr error;
        std::string res="";
        try{
            res=mergeChunks();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,error,res,handle]{
            if(error){
                setException(error);
            }else{
                setValue(res);
            }
            handle.resume();
        });
    });
    if(!success){
        setValue("");
        handle.resume();
    }
}

RemoveTaskDirectoryAwaiter::RemoveTaskDirectoryAwaiter(const std::string &uploadId):
    path(TaskPath(uploadId)){};

bool RemoveTaskDirectoryAwaiter::removeTaskDirectory(){
    if(path.empty())return false;
    std::error_code ec;
    fs::remove_all(path,ec);
    if(ec){
        // 说明路径存在的同时删除失败
        return false;
    }
    // 成功删除或者路径本身不存在
    return true;
}

void RemoveTaskDirectoryAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop]{
        std::exception_ptr error;
        bool ans=false;
        try{
            ans=removeTaskDirectory();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,error,ans,handle]{
            if(error){
                setException(error);
            }else{
                setValue(ans);
            }
            handle.resume();
        });
    });
    if(!success){
        setValue(false);
        handle.resume();
    }
}

namespace upload::storage{
    std::string CreateUploadId(int n){
        if(n<=0)return {};
        constexpr char hex[]="0123456789abcdef";
        std::string res;
        res.reserve(static_cast<size_t>(n)*2);
        for(int i=0;i<n;){
            unsigned char tmp;
            const ssize_t bytesRead=getrandom(&tmp,sizeof tmp,0);
            if(bytesRead!=1){
                if(bytesRead<0 && errno==EINTR)continue;
                return {};
            }
            res+=hex[tmp>>4];
            res+=hex[tmp&0x0f];
            ++i;
        }
        return res;
    }
    Task<bool> CreateTaskDirectory(const std::string& uploadId){
        co_return co_await CreateTaskDirectoryAwaiter{uploadId};
    }
    Task<bool> SaveChunk(const std::string& uploadId,uint64_t index,const HttpFile&chunk){
        if(!IsUploadId(uploadId) || index==0)co_return false;
        auto redis=app().getRedisClient();
        if(!redis)co_return false;
        if(!(co_await SaveChunkAwaiter{uploadId,index,chunk}))co_return false;

        const auto taskKey=UTS_KEY+uploadId;
        const auto metaKey=taskKey+":meta";
        const auto chunksKey=taskKey+":chunks";
        const auto part=std::to_string(index);
        constexpr char recordChunk[]=
            "if redis.call('EXISTS', KEYS[1]) == 0 then return 0 end "
            "redis.call('SADD', KEYS[2], ARGV[1]) "
            "return 1";
        try{
            const auto result=co_await redis->execCommandCoro(
                "EVAL %s 2 %s %s %s",recordChunk,metaKey.c_str(),chunksKey.c_str(),part.c_str()
            );
            co_return result.type()==nosql::RedisResultType::kInteger && result.asInteger()==1;
        }catch(const nosql::RedisException& e){
            LOG_ERROR<<"redis error:"<<e.what();
            co_return false;
        }
    }
    Task<std::string> MergeChunks(const std::string& uploadId,uint64_t totalChunkes,uint64_t expectedSize){
        co_return co_await MergeChunksAwaiter{uploadId,totalChunkes,expectedSize};
    }
    Task<bool> RemoveTaskDirectory(const std::string& uploadId){
        co_return co_await RemoveTaskDirectoryAwaiter{uploadId};
    }
    Task<Json::Value> CreateTask(const std::string&uploadId,Json::Value metadata){
        Json::Value json;
        json["status"]=false;
        if(!IsUploadId(uploadId)){
            json["error"]=UTS_BAD_ID;
            co_return json;
        }

        const auto taskKey=UTS_KEY+uploadId;
        const auto metaKey=taskKey+":meta";
        const auto chunksKey=taskKey+":chunks";
        const auto ownerId=metadata["owner_id"].asString();
        const auto parentId=metadata["parent_id"].asString();
        const auto name=metadata["name"].asString();
        const auto size=metadata["size"].asString();
        const auto lastModified=metadata["last_modified"].asString();
        const auto chunkSize=metadata["chunk_size"].asString();
        const auto totalChunks=metadata["total_chunks"].asString();

        auto redis=app().getRedisClient();
        if(!redis){
            json["error"]=UTS_REDIS_ERR;
            co_return json;
        }
        if(!(co_await CreateTaskDirectory(uploadId))){
            json["error"]="DIRECTORYCREATEERROR";
            co_return json;
        }

        constexpr char createMeta[]=
            "if redis.call('EXISTS', KEYS[1], KEYS[2]) ~= 0 then return 0 end "
            "redis.call('HSET', KEYS[1], "
            "'owner_id', ARGV[1], 'parent_id', ARGV[2], 'name', ARGV[3], "
            "'size', ARGV[4], 'last_modified', ARGV[5], "
            "'chunk_size', ARGV[6], 'total_chunks', ARGV[7]) "
            "return 1";
        bool removeNewDirectory=false;
        json["error"]=UTS_REDIS_ERR;
        try{
            auto result=co_await redis->execCommandCoro(
                "EVAL %s 2 %s %s %s %s %b %s %s %s %s",
                createMeta,metaKey.c_str(),chunksKey.c_str(),
                ownerId.c_str(),parentId.c_str(),name.data(),name.size(),
                size.c_str(),lastModified.c_str(),chunkSize.c_str(),totalChunks.c_str()
            );
            if(result.type()==nosql::RedisResultType::kInteger){
                const auto created=result.asInteger();
                if(created==1){
                    json.removeMember("error");
                    json["status"]=true;
                    co_return json;
                }
                if(created==0){
                    json["error"]="KEYEXISTS";
                    removeNewDirectory=true;
                }
            }
        }catch(const nosql::RedisException& e){
            LOG_ERROR<<"redis error:"<<e.what();
            // 超时或断线时命令可能已执行，保留目录以免已写入的元数据指向空路径。
            removeNewDirectory=e.code()==nosql::RedisErrorCode::kRedisError ||
                e.code()==nosql::RedisErrorCode::kNoConnectionAvailable;
        }
        if(removeNewDirectory && !(co_await RemoveTaskDirectory(uploadId))){
            LOG_ERROR<<"remove new task directory error:"<<uploadId;
        }
        co_return json;
    }
    Task<Json::Value> GetTask(const std::string& uploadId){
        Json::Value json;
        json["status"]=false;
        if(!IsUploadId(uploadId)){
            json["error"]=UTS_BAD_ID;
            co_return json;
        }

        auto redis=app().getRedisClient();
        json["error"]=UTS_REDIS_ERR;
        if(!redis)co_return json;
        const auto taskKey=UTS_KEY+uploadId;
        const auto metaKey=taskKey+":meta";
        const auto chunksKey=taskKey+":chunks";
        constexpr char readTask[]=
            "local meta = redis.call('HGETALL', KEYS[1]) "
            "if #meta == 0 then return false end "
            "return {meta, redis.call('SMEMBERS', KEYS[2])}";
        try{
            const auto result=co_await redis->execCommandCoro(
                "EVAL %s 2 %s %s",readTask,metaKey.c_str(),chunksKey.c_str()
            );
            if(result.isNil()){
                json["error"]="NOEXISTS";
                co_return json;
            }
            if(result.type()!=nosql::RedisResultType::kArray)co_return json;
            const auto task=result.asArray();
            Json::Value metadata(Json::objectValue);
            const auto fields=task[0].asArray();
            for(size_t i=0;i<fields.size();i+=2){
                metadata[fields[i].asString()]=fields[i+1].asString();
            }

            const auto members=task[1].asArray();
            std::vector<uint64_t> indices;
            indices.reserve(members.size());
            for(const auto& member:members){
                const auto text=member.asString();
                const auto end=text.data()+text.size();
                uint64_t index=0;
                const auto [ptr,ec]=std::from_chars(text.data(),end,index);
                if(ec!=std::errc{} || ptr!=end || index==0)co_return json;
                indices.push_back(index);
            }
            std::sort(indices.begin(),indices.end());
            Json::Value uploadedChunks(Json::arrayValue);
            for(auto index:indices)uploadedChunks.append(Json::UInt64(index));

            json["metadata"]=std::move(metadata);
            json["uploaded_chunks"]=std::move(uploadedChunks);
            json["status"]=true;
            json.removeMember("error");
        }catch(const nosql::RedisException& e){
            LOG_ERROR<<"redis error:"<<e.what();
        }
        co_return json;
    }

    Task<Json::Value> RemoveTask(const std::string& uploadId){
        Json::Value json;
        json["status"]=false;
        if(!IsUploadId(uploadId)){
            json["error"]=UTS_BAD_ID;
            co_return json;
        }

        auto redis=app().getRedisClient();
        json["error"]=UTS_REDIS_ERR;
        if(!redis)co_return json;
        const auto taskKey=UTS_KEY+uploadId;
        const auto metaKey=taskKey+":meta";
        const auto chunksKey=taskKey+":chunks";
        try{
            co_await redis->execCommandCoro(
                "DEL %s %s",metaKey.c_str(),chunksKey.c_str()
            );
        }catch(const nosql::RedisException& e){
            LOG_ERROR<<"redis error:"<<e.what();
            co_return json;
        }

        if(!(co_await RemoveTaskDirectory(uploadId))){
            json["error"]="DIRECTORYCLEARERROR";
            co_return json;
        }
        json["status"]=true;
        json.removeMember("error");
        co_return json;
    }
}
