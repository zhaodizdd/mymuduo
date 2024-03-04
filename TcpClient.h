#pragma once

#include "noncopyable.h"
#include "TcpConnection.h"

#include <memory>
#include <atomic>
#include <mutex>

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable
{
public:
    TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &nameArg);
    ~TcpClient();

    // 发起连接
    void connect();
     //用于连接已建立的情况下，关闭连接
    void disconnect();
    //连接尚未建立成功，停止发起连接
    void stop();

    // 获取当前连接的connection
    TcpConnectionPtr connection() const
    {
        std::unique_lock<std::mutex> (mutex_);
        return connection_;
    }

    EventLoop* getLoop() const { return loop_ ; }
    bool retry() const { return retry_; }
    // 设置客户端连接建立之后又断开的时候可以重连
    void enableRetry() { retry_ = true; }

    const std::string& name() const { return name_; } 

    // 注册连接建立成功时回调的函数
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = std::move(cb); 
    }
    // 注册消息到来时的回调函数
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = std::move(cb);
    }
    // 注册数据发送完毕时的回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = std::move(cb);
    }

private:
    // 连接建立成功时回调的函数
    void newConnection(int sockfd);
    // 连接断开时回调的函数 具有重连功能
    void removeConnection(const TcpConnectionPtr &conn);

    EventLoop *loop_; 
    ConnectorPtr connector_;    // 用于主动发起连接
    const std::string name_;

    //连接建立成功时的回调函数
    ConnectionCallback connectionCallback_;
    //消息到来时的回调函数
    MessageCallback messageCallback_;
    //数据发送完毕时的回调函数
    WriteCompleteCallback writeCompleteCallback_;

    //是否重连，是指连接建立之后又断开的时候是否重连
    std::atomic_bool retry_;
    //是否连接
    std::atomic_bool connect_;

    //下一个连接的ID，name_ + nextConnId_用于标识一个连接
    int nextConnId_;

    std::mutex mutex_;
    //Connector连接成功以后，得到一个TcpConnection类型的connection_
    TcpConnectionPtr connection_;
};