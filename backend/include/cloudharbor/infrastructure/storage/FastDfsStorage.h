#pragma once

#include <drogon/drogon.h>
#include <coroutine>
#include "ThreadPool.h"
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstdio>
using namespace drogon;
class UploadAwaiter:public CallbackAwaiter<std::string>{
private:
    std::string path;
    const std::string conf;
    std::string uploadFile();
public:
    UploadAwaiter(const std::string& p);
    void await_suspend(std::coroutine_handle<> handle);
    ~UploadAwaiter()=default;
};

class DeleteAwaiter:public CallbackAwaiter<bool>{
private:
    std::string storageId;
    const std::string conf;
    bool deleteFile();
public:
    DeleteAwaiter(const std::string& id);
    void await_suspend(std::coroutine_handle<> handle);
    ~DeleteAwaiter()=default;
};

class FastDfsStorage{
public:
    static FastDfsStorage& getInstance();
    Task<std::string> uploadFile(const std::string& localPath);
    Task<bool> deleteFile(const std::string& storageId);
    
    FastDfsStorage(const FastDfsStorage&x)=delete;
    FastDfsStorage& operator=(const FastDfsStorage&x)=delete;
private:
    FastDfsStorage()=default;
    ~FastDfsStorage()=default;
};