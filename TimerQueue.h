#pragma once

#include "noncopyable.h"
#include "Callbacks.h"
#include "Timestamp.h"
#include "Channel.h"

#include <set>
#include <atomic>
#include <vector>

class EventLoop;
class Timer;
class TimerId;

class TimerQueue
{
public:

    explicit TimerQueue(EventLoop *loop);
    ~TimerQueue();

    // 增加定时器
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    // 取消定时器
    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    // set存储Entry Timestamp 类型需要支持比较操作符（operator<）
    // Timer* 指针类型应该可以进行正确的比较操作但是只是比较的指针地址不是比较的指针指向的对象
    using TimerList = std::set<Entry>; // set容器存储所有的<Timestamp, Timer*>
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer *timer);
    void cancelInLoop(TimerId timerId);

    //当定时器超时时，文件描述符触发可读事件
    //poll() 中会调用 Channel 的 handleEvent()，而 handleEvent() 触发该事假
    void handleRead();

    // 获取所有超时的定时器
    std::vector<Entry> getExpired(Timestamp now);
    // 重新设置超时定时器 如果是重复执行定时器就再入队，否则删除
    void reset(const std::vector<Entry> &expired, Timestamp now);

    // 把定时器插入到Timers_和activeTimers_(cancel)
    // 返回值是 判断新插入的定时器的过期时间是否更早
    bool insert(Timer* timer);

    EventLoop *loop_;           
    const int timerfd_;         // 定时器描述符
    Channel timerfdChannel_;    // 定时器的channel
    TimerList timers_;          // timers_ 是按超时时间戳排序，对于相同时间戳按 Timer* 地址进行排序

    ActiveTimerSet activeTimers_;               //是按照定时器地址排序
    std::atomic_bool callingExpiredTimers_;     //是否在处理超时定时器
    ActiveTimerSet cancelingTimers_;            //保存被取消的定时器
};

