#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"


#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接， 通过accept函数拿到connfd
 *
 * =>  TcpConnection 设置回调 =》 Channel =》Poller =》 Channel的回调操作
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
private:
    enum StateE // 连接状态
    {
        kDisconnected,  // 底层的sockt已关闭完
        kConnecting,    // 初始状态
        kConnected,     // 连接成功
        kDisconnecting  // 断开连接
    };
public:
    TcpConnection(EventLoop *loop,
                  const std::string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &PeerAddr);

    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress & peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    // 设置各事件的回调
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }   
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }   
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb){ highWaterMarkCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }

    // 建立连接
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
    // 设置连接的连接状态
    void setState(StateE state) { state_ = state;}
    // 强行关闭tcpConnection 
    void forceClose();
private:

    // 各事件的处理方法
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    //这个回调是给TcpServer和TcpClient用的，用于通知它们移除所持有的TcpConnectionPtr
    void handleClose(); 
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

    EventLoop *loop_; // 这里绝对不是baseLoop 因为TcpConnection都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_; // 连接状态
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;   // 对方的地址信息
    const InetAddress peerAddr_;    // 自己的地址信息

    ConnectionCallback connectionCallback_;       // 有新连接时的回调函数
    MessageCallback messageCallback_;             // 有读写消息时的回调函数
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成时的回调函数
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;  // 高水位防止一端数据发送的太快，另一端接受数据太慢设置的一个阈值

    Buffer inputBuffer_;    // 接受数据的缓冲区
    Buffer outputBuffer_;   // 发送数据的缓冲区
};