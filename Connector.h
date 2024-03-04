#pragma once

#include "noncopyable.h"
#include "InetAddress.h"

#include <atomic>
#include <functional>
#include <memory>

class Channel;
class EventLoop;

class Connector : noncopyable, public std::enable_shared_from_this<Connector>
{
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(EventLoop *loop, const InetAddress &serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    void start();   // 开始连接 可以在任何线程中调用
    void restart(); // 重新连接 必须在循环线程中调用
    void stop();    // 暂停 可以在任何线程中调用

    const InetAddress &serverAddress() const { return serverAddr_; }

private:
    enum States
    {
        kDisconnected, // 连接断开
        kConnecting,   // 连接中
        kConnected     // 连接成功
    };

    static const int kMaxRetryDelayMs;
    static const int kInitRetryDelayMs;

    void setState(States state) { state_ = state; }
    void startInLoop();

    void stopInLoop(); // 在当前loop下停止

    void connect();
    void connecting(int sockfd);

    void handleWrite();
    void handleError();

    // 连接失败后，重连延迟时间后重连
    void retry(int sockfd);
    // 释放用于连接的Channel
    int removeAndResetChannel();
    // 释放Channel
    void resetChannel();

    EventLoop *loop_;
    InetAddress serverAddr_; // 服务器地址信息

    std::atomic_bool connect_; // 是否连接
    std::atomic_int state_; // 连接状态
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;   // //连接成功时的回调函数
    int retryDelayMs_; // 连接失败时的重连延迟时间
};