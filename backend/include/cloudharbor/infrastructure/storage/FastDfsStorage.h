#pragma once

#include <fastdfs/fdfs_client.h>
#include <drogon/drogon.h>
using namespace drogon;

class FastDfsStorage{
public:
    FastDfsStorage();
    Task<std::string> uploadFile(const std::string& localPath);
    Task<bool> deleteFile(const std::string& storageId);
    ~FastDfsStorage();
private:

};