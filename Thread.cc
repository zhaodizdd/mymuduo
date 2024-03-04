#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h> //信号量

std::atomic_int Thread::numCreated_{0};

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefualtName();
}
Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();  // thread类提供的设置分离线程的方法
    }
}

void Thread::start()    // 一个Thread对象， 记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    thread_ = std::make_shared<std::thread>([&](){
        // 获取线程tid的值
        tid_ = CurrentThread::tid();
        sem_post(&sem); //利用semaphore阻塞等待新线程获取tid
        func_(); // 开启一个新线程，专门执行该线程函数
    });
    // 这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem);
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefualtName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}