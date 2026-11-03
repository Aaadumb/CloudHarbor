#include "ThreadPool.h"
#include <cstdio>
#include <exception>

ThreadPool& ThreadPool::getInstance(){
    static ThreadPool instance;
    return instance;
}
ThreadPool::ThreadPool(int n){
    for(int i=0;i<n;i++){
        threads.emplace_back([&](){
            while(true){
                std::unique_lock<std::mutex> lock(mtx);
                condition.wait(lock,[&](){
                    return !tasks.empty() || stop;
                });
                if(stop && tasks.empty())break;
                auto task=std::move(tasks.front());
                tasks.pop();
                lock.unlock();
                try{
                    task();
                }catch(const std::exception& e){
                    // 任务应自行传回业务结果，这里兜底避免工作线程异常退出。
                    std::fprintf(stderr,"ThreadPool task failed: %s\n",e.what());
                }catch(...){
                    std::fputs("ThreadPool task failed: unknown exception\n",stderr);
                }
            }
        });
    }
}
ThreadPool::~ThreadPool(){
    std::unique_lock<std::mutex> lock(mtx);
    stop=true;
    lock.unlock();
    condition.notify_all();
    for(auto& t:threads)t.join();
}
