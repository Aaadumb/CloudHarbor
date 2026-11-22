#pragma once

#include <drogon/drogon.h>

class TransactionCommitAwaiter:public drogon::CallbackAwaiter<bool>{
public:
    explicit TransactionCommitAwaiter(std::shared_ptr<drogon::orm::Transaction>& tx):tx(tx){}

    void await_suspend(std::coroutine_handle<> handle){
        auto* loop=trantor::EventLoop::getEventLoopOfCurrentThread();
        tx->setCommitCallback([this,loop,handle](bool committed){
            loop->queueInLoop([this,handle,committed]{
                setValue(committed);
                handle.resume();
            });
        });
        tx.reset();
    }

private:
    std::shared_ptr<drogon::orm::Transaction>& tx;
};
