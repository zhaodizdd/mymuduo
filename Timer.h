#pragma once

#include "Callbacks.h"
#include "noncopyable.h"
#include "Timestamp.h"

#include <atomic>

class Timer : noncopyable
{
public:
    // 参数：
    //1 定时器回调函数 
    //2.定时器下一次的终止时间，
    //3.超时时间间隔， 如果是一次性定时器， 该值为0
    Timer(TimerCallback cb, Timestamp when, double interval);

    // 调用回调函数，即创建对象时的函数
    void run() const { callback_(); }

    // 返回下一次的终止时间
    Timestamp expiration() const { return expiration_; }
    // 查看定时器是否重复
    bool repeat() const { return repeat_; }
    // 查看定时器序号 
    int64_t sequence() const { return sequence_; }

    //重启计数器
    void restart(Timestamp now);

    static int64_t numCreated() { return static_cast<int64_t>(s_numCreated_);}

private:
    const TimerCallback callback_;  // 定时器的回调函数
    Timestamp expiration_;          // 下一次的终止时间
    const double interval_;         // 超时时间间隔， 如果是一次性定时器， 该值为0
    const bool repeat_;             // 是否重复
    const int64_t sequence_;        // 定时器的序号

    static std::atomic_int64_t s_numCreated_;  // 定时器的数量
};