#pragma once

/**
 * 用户使用muduo编写服务器程序
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>


// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option //表示是否对端口可重用
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg , Option option = kNoReusePort);
    ~TcpServer();

    const std::string& ipPort() const { return ipPort_; }
    const std::string& name() const { return name_; }
    EventLoop* getLoop() const { return loop_; }

    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; } 
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器的监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop *loop_;   // baseLoop 用户定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;    // 运行在mainLoop，任务：监听新的连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_;   // noe loop per thread

    ConnectionCallback connectionCallback_; // 有新连接时的回调函数
    MessageCallback messageCallback_;   // 有读写消息时的回调函数
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成时的回调函数

    ThreadInitCallback threadInitCallback_; // loop线程初始化时的回调函数
    std::atomic_int started_;   //表示TcpServer设否开启初始为未开启0

    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接
};