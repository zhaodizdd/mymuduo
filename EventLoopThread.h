#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThraedInitCallback = std::function<void(EventLoop *)>;
    
    EventLoopThread(const ThraedInitCallback &cb = ThraedInitCallback(),
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadFunc(); //线程函数，在里面创建loop 
    
    EventLoop *loop_;
    bool exiting_;  //线程是否退出
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThraedInitCallback callback_;   //初始设置的回调函数
};