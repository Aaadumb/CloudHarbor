#include "FastDfsStorage.h"

UploadAwaiter::UploadAwaiter(const std::string& p):path(p),conf(getenv("FDFSCLIENTCONF")){};

std::string UploadAwaiter::uploadFile(){
    // 错误返回空字符串
    int fd[2];
    if(pipe(fd)==-1){
        // 管道创建失败
        return "";
    }

    pid_t pid=fork();
    if(pid==-1){
        close(fd[0]);
        close(fd[1]);
        return "";
    }
    
    if(pid==0){
        // 子进程 负责写
        close(fd[0]);
        if(dup2(fd[1],STDOUT_FILENO)==-1){
            _exit(127);
        }
        if(fd[1]!=STDOUT_FILENO){
            close(fd[1]);
        }
        execlp("fdfs_upload_file","fdfs_upload_file",conf.c_str(),path.c_str(),NULL);
        // 如果出错会执行下面的代码
        _exit(127);
    }
    // 主进程负责读
    close(fd[1]);
    
    std::string res(BUFSIZ,' ');
    ssize_t n;
    do{
        n=read(fd[0],&res[0],BUFSIZ);
    }while(n==-1 && errno==EINTR);
    
    close(fd[0]);

    int status=0;
    pid_t result;

    do{
        result=waitpid(pid,&status,0);
    }while(result==-1 && errno==EINTR);

    if(n<=0 
        || result!=pid 
        || !WIFEXITED(status) 
        || WEXITSTATUS(status)!=0){
        return "";
    }

    res.resize(n);
    while(!res.empty() && 
        (res.back()=='\n' || res.back()=='\r')){
            res.pop_back();
    }
    return res;
}

void UploadAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto* loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop]{
        std::string res="";
        std::exception_ptr error;
        try{
            res=uploadFile();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([=]{
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

Task<std::string> FastDfsStorage::uploadFile(const std::string& path){
    co_return co_await UploadAwaiter{path};
}

FastDfsStorage& FastDfsStorage::getInstance(){
    static FastDfsStorage instance;
    return instance;
}

DeleteAwaiter::DeleteAwaiter(const std::string& id):storageId(id),conf(getenv("FDFSCLIENTCONF")){};

bool DeleteAwaiter::deleteFile(){
    pid_t pid=fork();
    if(pid==-1){
        return false;
    }
    if(pid==0){
        execlp("fdfs_delete_file","fdfs_delete_file",conf.c_str(),storageId.c_str(),NULL);
        _exit(127);
    }
    int status=0;
    pid_t result;
    do{
        result=waitpid(pid,&status,0);
    }while(result==-1 && errno==EINTR);
    if(result==-1){
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
}
void DeleteAwaiter::await_suspend(std::coroutine_handle<>handle){
    auto* loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop]{
        bool res=false;
        std::exception_ptr error;
        try{
            res=deleteFile();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([=]{
            if(error){
                setException(error);
            }
            else{
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

Task<bool> FastDfsStorage::deleteFile(const std::string& storageId){
    co_return co_await DeleteAwaiter{storageId};
}