#include "TcpClient.h"
#include "Logger.h"
#include "Connector.h"
#include "EventLoop.h"
#include "InetAddress.h"

#include <cstring>

namespace detail
{

    // 这里的removeConnection最终是TcpConnection的CloseCallback回调函数，不需要重连功能
    // 从传入的loop中删除传入的tcpconnection
    static void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn)
    {
        loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 检查主loop是否存在，若不存在程序无法执行下去
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpClient::TcpClient(EventLoop *loop,
                     const InetAddress &serverAddr,
                     const std::string &nameArg)
    : loop_(CheckLoopNotNull(loop))
    , connector_(new Connector(loop, serverAddr))
    , name_(nameArg)
    , connectionCallback_()
    , messageCallback_()
    , retry_(false)
    , connect_(true)
    , nextConnId_(1)
{
    // 设置连接成功后的回调函数
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, std::placeholders::_1)
    );
    LOG_INFO("TcpClient::TcpClient[%s] - connector %s",
             name_.c_str(), connector_->serverAddress().toIpPort().c_str());
}
TcpClient::~TcpClient()
{
    LOG_INFO("TcpClient::~TcpClient[%s] - connector %s",
             name_.c_str(), connector_->serverAddress().toIpPort().c_str());

    TcpConnectionPtr conn;
    bool unique = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }
    if (conn)
    {
        if (loop_ == conn->getLoop())
        {
            // 重新设置TcpConnection中的closeCallback_为detail::removeConnection
            // bind 后cb = removeConnection(loop_, ConnectionPtr);
            CloseCallback cb = std::bind(&detail::removeConnection, loop_, std::placeholders::_1);
            loop_->runInLoop(
                std::bind(&TcpConnection::setCloseCallback, conn, cb));
        }
        if (unique)
        {
            conn->forceClose();
        }

    }
    else
    {
        // 这种情况表示connector处于未连接状态，将connector_停止
        connector_->stop();
    }
}

// 发起连接
void TcpClient::connect()
{
    LOG_INFO("TcpClien::connect[%s] - connecting to %s",
             name_.c_str(), connector_->serverAddress().toIpPort().c_str());
    connect_ = true;
    connector_->start();
}

// 用于连接已建立的情况下，关闭连接
void TcpClient::disconnect()
{
    connect_ = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connection_)
        {
            connection_->shutdown();
        }
    }
}
// 连接尚未建立成功，停止发起连接
void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
    if (!loop_->isInLoopThread())
    {
        LOG_FATAL("The EventLoop object is not in its own thread");
    }
    else
    {
        // 通过已连接的sockfd获得对方的IP信息
        sockaddr_in peer;
        socklen_t addrlen = sizeof peer;
        ::memset(&peer, 0, sizeof peer);
        if (::getpeername(sockfd, (sockaddr *)&peer, &addrlen) < 0)
        {
            LOG_ERROR("sockets::getPeerAddr");
        }
        InetAddress peerAddr(peer);
        
        char buf[32];
        snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
        ++nextConnId_;
        std::string connName = name_ + buf;

        sockaddr_in local;
        ::memset(&local, 0, sizeof local);
        if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
        {
            LOG_ERROR("sockets::getLocalAddr");
        }
        InetAddress localAddr(local);

        // 创建TcpConnection对象
        TcpConnectionPtr conn(new TcpConnection(loop_,
                                                connName,
                                                sockfd,
                                                localAddr,
                                                peerAddr));
        // 设置回调函数
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        conn->setCloseCallback(
            std::bind(&TcpClient::removeConnection, this, std::placeholders::_1)
        );

        {
            std::unique_lock<std::mutex> lock(mutex_);
            connection_ = conn;
        }
        conn->connectEstablished();
    }
}


void TcpClient::removeConnection(const TcpConnectionPtr &conn)
{
    if (!loop_->isInLoopThread())
    {
        LOG_FATAL("The EventLoop object is not in its own thread");
    }
    else
    {
        if(loop_ != conn->getLoop());
        {
            LOG_FATAL("%s:%s:%d not the current loop ",__FILE__, __FUNCTION__, __LINE__);
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            connection_.reset();
        }

        loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

        // 如果设置
        if (retry_ && connect_)
        {
            LOG_INFO("TcpClient::connect[ %s ] - Reconnecting to %s",
                     name_.c_str(), connector_->serverAddress().toIpPort().c_str());
            connector_->restart();
        }
    }
}