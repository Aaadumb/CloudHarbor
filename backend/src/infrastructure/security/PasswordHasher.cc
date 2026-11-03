#include "PasswordHasher.h"
#include <exception>
#include <utility>

PasswordHasher& PasswordHasher::getInstance(){
    static PasswordHasher instance;
    return instance;
}

Task<std::string> PasswordHasher::hashPassword(const std::string& pwd){
    co_return co_await HasherAwaiter{pwd};
}

Task<bool> PasswordHasher::verifyPassword(const std::string& pwd,const std::string& passwordHash){
    co_return co_await VerifyAwaiter{pwd,passwordHash};
}

HasherAwaiter::HasherAwaiter(const std::string& p):pwd(p){};

void HasherAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto *loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop](){
        std::string res;
        std::exception_ptr error;
        try{
            res=hashPassword();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,handle,res=std::move(res),error]() mutable {
            if(error){
                setException(error);
            }else{
                setValue(std::move(res));
            }
            handle.resume();
        });
    });
    if(!success){
        setValue("");
        handle.resume();
    }
}

std::string HasherAwaiter::hashPassword(){
    // 错误则会返回空字符串
    std::string out(crypto_pwhash_STRBYTES,' ');
    if(crypto_pwhash_str(&out[0],
                        &pwd[0],pwd.size(),
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0){
        // 说明出错 或者内存不足
        out.clear();
    }else{
        // libsodium写入以'\0'结尾的文本，不会更新string自身的长度。
        out.resize(out.find('\0'));
    }
    return out;
}

VerifyAwaiter::VerifyAwaiter(const std::string&p,const std::string& h):pwd(p),hash(h){};

bool VerifyAwaiter::verifyPassword(){
    return crypto_pwhash_str_verify(hash.c_str(),pwd.c_str(),pwd.size()) == 0;
}

void VerifyAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto *loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop]{
        bool res=false;
        std::exception_ptr error;
        try{
            res=verifyPassword();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,handle,res,error]{
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
