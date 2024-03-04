#include "Timer.h"

std::atomic_int64_t s_numCreated_{0};
static int incrementAndGet()
{
    return  ++s_numCreated_;
}

Timer::Timer(TimerCallback cb, Timestamp when, double interval)
    : callback_(std::move(cb))
    , expiration_(when)
    , interval_(interval)
    , repeat_(interval > 0.0)
    , sequence_(incrementAndGet())
{}

void Timer::restart(Timestamp now)
{
    if (repeat_)
    {
        expiration_ = addTime(now, interval_);
    }
    else
    {
        expiration_ = Timestamp::invalid();
    }
}