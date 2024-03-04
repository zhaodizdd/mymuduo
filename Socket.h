#pragma once
#include "noncopyable.h"
class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite(); //关闭fd的写端

    // 设置套接字描述符的属性
    void setTcpNoDelay(bool on);    // 设置Tcp是否关闭Nagle算法
    void setReuseAddr(bool on); // 设置地址复用功能
    void setReusePort(bool on); // 
    void setKeepAlive(bool on); // 套接字保活
private:
    const int sockfd_;
};