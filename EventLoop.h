#pragma once 

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include "TimerId.h"
#include "Callbacks.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;
class TimerQueue;

// 事件循环类 主要包括两个模块 Channel Poller (epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cp放入队列中， 唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    TimerId runAt(Timestamp time, TimerCallback cb);

    TimerId runAfter(double delay, TimerCallback cb);

    TimerId runEvery(double interval, TimerCallback cb);

    void cancel(TimerId timerId);

    // 用来唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法 =》 poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();};
private:
    void handleRead();  // wake up
    // 执行当前loop需要执行的回调操作
    void doPendingFunctors();   

    using ChannelList = std::vector<Channel*>;
    
    std::atomic_bool looping_;  // 是否开启事件循环 原子操作， 通过CAS实现
    std::atomic_bool quit_; // 标识退出loop循环

    const pid_t threadId_; // 记录当前loop的线程id

    Timestamp pollReturnTime_;  // poller返回事件的channels时间点
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_;  // 主要作用，当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;    // 注册到loop上的Channel集合

    std::atomic_bool callingPendingFunctors_;   // 标识当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;  // 存储loop需要执行的所以的回调操作
    std::mutex mutex_; // 互斥锁， 用来保护上面vector容器的线程安全操作
};
