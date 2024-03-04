#include "TimerQueue.h"
#include "Logger.h"
#include "Timer.h"
#include "TimerId.h"
#include "EventLoop.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <assert.h>

#include <cstring>

static int createTimerfd()
{
    // CLOCK_MONOTONIC不可设置的时钟不会受到系统时间变化的影响
    // TFD_NONBLOCK | TFD_CLOEXEC 和创建sockfd应用设置tfd不阻塞
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_FATAL("Failed in timerfd_create");
    }
    return timerfd;
}

// 计算多久后计算机超时 (超时时刻（when） - 现在时刻)
static struct timespec howMuchTimeFromNow(Timestamp when)
{
    // (超时时刻（when） - 现在时刻) 的时间戳
    int64_t microseconds = when.microSecondsSinceEpoch()
                            - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    // 该结构体有两个值 包括秒(tv_sec)和纳秒(tv_nsec)
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

// 重置Timerfd中的到期时间
static void resetTiemrfd(int timerfd, Timestamp expiration)
{
    // itimerspec 存储了两个值 it_interval(周期性间隔) it_value(初始到期时间)
    struct itimerspec newValue;
    struct itimerspec oldValue;
    ::memset(&newValue, 0, sizeof newValue);
    ::memset(&oldValue, 0, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);

    // 启动 或 停止文件描述符指向的定时器.
    // newValue参数指定定时器初始的到期时间间隔和间隔.
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        LOG_FATAL("timerfd_settime()");
    }
}

static void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_INFO("TimerQuue::handleRead() %ld at %s", howmany, now.toString().c_str());
    if (n != sizeof howmany)
    {
        LOG_ERROR("TimerQueue::handleRead() reads %zd bytes instead of 8", n);
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(createTimerfd())
    , timerfdChannel_(loop, timerfd_)
    , timers_()
    , callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this)
    );
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
}

// 增加定时器
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    // earliestChanged 表示判断插入的定时器的过期时间是否
    // 不列表中的最小定时器小
    bool earliestChanged = insert(timer);

    if (earliestChanged)
    {
        // 如果新入队的定时器是队列里最早的，从新设置下系统定时器到期触发时间
        resetTiemrfd(timerfd_, timer->expiration());
    }
}

// 把定时器插入到Timers_和activeTimers_(cancel)
// 返回值是 判断新插入的定时器的过期时间是否更早
bool TimerQueue::insert(Timer *timer)
{
    // timers_是按过期时间有序排列的，最早到期的在前面
    // 新插入的时间和队列中最早到期时间比，判断新插入时间是否更早
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();

    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    // 将时间保存入队，std::set自动保存有序
    {
        std::pair<TimerList::iterator, bool> result =
            timers_.insert(Entry(when, timer));
        assert(result.second); (void)result;
    }
    {
        auto result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second); (void)result;
    }

    return earliestChanged;
}

// 取消定时器
void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    // 通过参数 timerId 构造 ActiveTimer 对象
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    auto it = activeTimers_.find(timer);

    //如果找到了该定时器，说明该定时器还没有到期
    if (it != activeTimers_.end())
    {
        // 从 timers_ 中erase 掉
        // set 的 erase 返回删除元素的个数
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        // erase只擦除元素，如果元素本身是一个指针，则不会以任何方式触及指向的内存。
        delete it->first;
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        cancelingTimers_.insert(timer);
    }
}

void TimerQueue::handleRead()
{
    Timestamp now(Timestamp::now());
    //定时器到期，读取一次timerfd_
    readTimerfd(timerfd_, now);
    // 获取所有到期的时间
    std::vector<Entry> expired = getExpired(now);

    // 处于定时器处理状态中
    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    //执行每个到期的定时器方法
    for(const Entry& it : expired)
    {
        it.second->run();
    }

    callingExpiredTimers_ = false;
    // 重置过期定时器状态，如果是重复执行定时器就再入队，否则删除
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> expired;
    // 新建一个当前时间定时器,
    // 用UINTPTR_MAX原因是默认排序
    // 如果时间相等则地址大小排序，取最大防止漏掉定时器
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    // 这里用到了std::set::lower_bound（寻找第一个大于等于Value的值），
    // 这里的Timer*是UINTPTR_MAX，所以返回的是一个大于Value的值，即第一个未到期的Timer的迭代器
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);
    // 拷贝出过期的定时器
    std::copy(timers_.begin(), end, back_inserter(expired));

    // 从已有的队列中清除
    // 从timers_中删除
    timers_.erase(timers_.begin(), end);
    // 从activeTimers_中删除
    for (const Entry& it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1); (void)n;
    }
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire; // 记入定时器下一次的超时时间

    for (const Entry& it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat()
            && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {   
            // 表示 1、定时器是重复定时器，2、取消定时器队列中无此定时器。
            //则此定时器再入队
            it.second->restart(now);
            insert(it.second);
        }
        else
        {
            delete it.second;
        }
    }

    if (!timers_.empty())
    {
        // 获取当前定时器队列中第一个（即最早过期时间）定时器
        nextExpire = timers_.begin()->second->expiration();
    }
    if (nextExpire.valid())
    {
        // 如果上面获取到了当前定时器队列中第一个定时器的超时时间
        // 重置timerfd(系统定时器)的到期时间
        resetTiemrfd(timerfd_, nextExpire);
    }
}