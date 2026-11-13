// 线程池 用于配合协程实现收发邮件等的异步协程处理
#pragma once
#include <vector>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>
// 整体采用单例模式 全局只有一个
class ThreadPool{
public:
    static ThreadPool& getInstance();
    ThreadPool& operator=(const ThreadPool&x)=delete;
    ThreadPool(const ThreadPool&x)=delete;
    template<class T,class ...Args>
    bool enqueue(T&& f,Args&& ...args){
        std::unique_lock<std::mutex> lock(mtx);
        if(stop)return false;
        tasks.emplace([x=std::forward<T>(f),...y=std::forward<Args>(args)](){
            x(y...);
        });
        condition.notify_one();
        return true;
    }
private:
    bool stop=false;
    std::mutex mtx;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::condition_variable condition;
    
    ThreadPool(int n=15);
    ~ThreadPool();
};