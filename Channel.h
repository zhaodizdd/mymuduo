#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * Eventloop Channel Poller之间的关系   《= Reactor模型上对应 Demultplex
 * Channel 理解为通道， 封装了sockfd和感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller换回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop_, int fd);
    ~Channel();

    // fd得到poller通知后， 处理事件的回调函数
    void handleEvent(Timestamp receiveTime);

    // 设置回调对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb);}

    // 防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    // 设置感兴趣的事件状态
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    // 设置fd可读
    void enableReading() { events_ |= kReadEvent; update(); }
    // 设置fd 不可读
    void disableReading() { events_ &= ~kReadEvent; update(); }
    // 设置fd 可写
    void enableleWriting() { events_ |= kWriteEvent; update(); }
    // 设置fd 不可写
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    // 设置fd 不可读写
    void disableAll() { events_ = kNoneEvent; update();}

    // 返回fd当前感兴趣的事件的状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveime);

    static const int kNoneEvent;    // 对所以事件都不感兴趣
    static const int kReadEvent;    // 对读事件感兴趣
    static const int kWriteEvent;   // 对写事件感兴趣

    EventLoop *loop_; // 事件循环
    const int fd_;      // fd , poller监听的对象
    int events_;    // 注册fd感兴趣的事件
    int revents_;   // poller返回的具体发生的事件
    int index_; // 判断channel是否注册在Poller中(kNew=-1 kAdded=1 kDeleted=2)

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为channel通道里面能够获知fd最终发生的具体事件revents
    // 所以它负责调用具体的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};