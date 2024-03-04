#include "Connector.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"

#include <netinet/in.h>
#include <errno.h>
#include <cstring>

const int Connector::kMaxRetryDelayMs = 30 * 1000; // 最大重试延迟时间 单位毫秒
const int Connector::kInitRetryDelayMs = 500;      // 初始重试延迟时间 单位毫秒

// 从socketfd获取SO_ERROR
static int getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof optval;
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }
    else
    {
        return optval;
    }
}

// 判断sockfd是否是自连接
static bool isSelfConnect(int sockfd)
{
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::memset(&local, 0, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    // 通过sockfd获取其绑定的对方的ip地址和端口信息
    sockaddr_in peer;
    ::memset(&peer, 0, sizeof peer);
    if (::getpeername(sockfd, (sockaddr*)&peer, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getPeerAddr");
    }

    return local.sin_port == peer.sin_port 
        && local.sin_addr.s_addr == peer.sin_addr.s_addr;
}

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{}
Connector::~Connector(){}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

void Connector::startInLoop()
{
    if (state_ == kDisconnected)
    {
        if (connect_)
        {
            connect();
        }
        else
        {
            LOG_DEBUG("do not connect!");
        }
    }
    else
        LOG_ERROR("The client not is disconnected.");
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}
void Connector::stopInLoop()
{
    if (state_ == kConnecting)
    {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

//连接，要处理三种状态。
//a、正在连接过程。
//b、重连。
//c、连接失败。
void Connector::connect()
{
    //创建非阻塞套接字
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        // 文件名：函数名：行数：错误信息
        LOG_FATAL("%s:%s:%d connect socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }

    // 发起连接
    int ret = ::connect(sockfd, (sockaddr *)serverAddr_.getSockAddr(), sizeof(sockaddr_in));
    int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS://非阻塞套接字，返回此状态，表示三次握手正在连接中
    case EINTR:
    case EISCONN: // 连接成功
      connecting(sockfd);
      break;

    //以下五种状态表示要重连
    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd); // 重连
      break;

    //余下状态直接关闭socket，不再进行连接
    case EACCES: //没权限
    case EPERM: //操作不允许
    case EAFNOSUPPORT: //地址族不被协议支持
    case EALREADY://操作已存在
    case EBADF: //错误文件描述符
    case EFAULT: //地址错误
    case ENOTSOCK: //非套接字上操作
      LOG_ERROR("connect error in Connector::startInLoop %d", savedErrno);
      ::close(sockfd);
      break;

    default:
      LOG_ERROR("Unexpected error in Connector::startInLoop %d", savedErrno);
      ::close(sockfd);
      break;
  }
}

// 重新开始连接
void Connector::restart()
{
    if (loop_->isInLoopThread())
    {
        setState(kDisconnected);
        retryDelayMs_ = kInitRetryDelayMs;
        connect_ = true;
        startInLoop();
    }
    else
    {
        LOG_FATAL("The EventLoop object is not in its own thread");
    }
}

//正在连接的处理。
//这里给正在连接的socket创建一个Channel去处理。
void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    if (!channel_)
    {
        LOG_FATAL("%s:%s:%d channel is not empty",__FILE__, __FUNCTION__, __LINE__);
    }
    //将该channel和EventLoop,socket关联
    //channel对应于一个文件描述符,所以在有了socket后才能创建channel
    channel_.reset(new Channel(loop_, sockfd));

    // 设置channel的回调操作
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));

    // 把channel的可写事件注册到Poller上
    channel_->enableleWriting();
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}

//连接发生错误时, socket 是可读可写的, 需要用 getsockopt 检查是否出错,
//这里需要对要对错误码做判断
//连接成功客户端会收到写事件。
//a、释放掉用于连接的Channel。
//b、有异常就重连。
//c、连接成功，检查下是否是自连接。(分析下什么是自连接。)
//d、正常的话就连接成功了。
//e、如果当前不是连接状态，就关闭此Socket
void Connector::handleWrite()
{
    LOG_INFO("Connector::handleWrite %d", static_cast<int>(state_));

    if (state_ == kConnecting)
    {
        //移除channel(Connector的channel只管理建立连接的阶段),成功建立连接后
        //交给TcpClient的TcpConnection来管理
        int sockfd = removeAndResetChannel();
        // 可写并不一定连接建立成功
        // 如果连接发生错误,socket会是可读可写的
        // 所以还需要调用getsockopt检查是否出错

        // 获取 错误码
        int err = getSocketError(sockfd);
        if (err)
        {
            LOG_ERROR("Connector::handleWrite - SO_ERROR = %d", err);
            retry(sockfd); // 重连
        }
        else if ( isSelfConnect(sockfd)) // 检测sockfd是否是自连接
        {
            LOG_ERROR("Connector::handleWrite - Self connect");
            retry(sockfd);
        }
        else
        {   
            // 这里表示连接成功更改状态
            // 调用TcpClient设置的回调函数,创建TcpConnection对象
            setState(kConnected);
            if (connect_) // 判断当前设置的状态 是否连接
            {
                // 在这里调用TcpClient注册的新连接成功后的回调函数创建一个TcpConnection对象
                newConnectionCallback_(sockfd); 
            }    
            else
            {
                ::close(sockfd);
                LOG_ERROR("do not connect");
            }
        }
    }
    else
    {
        if (state_ != kConnected)
        {
            LOG_FATAL("Connector::handleWrite - Not kConnected");
        }
    }
}
void Connector::handleError()
{
    LOG_ERROR("Connector::handleError state = %d", static_cast<int>(state_));
    if (state_ == kConnected)
    {
        int sockfd = removeAndResetChannel(); // 重置channel
        int err = getSocketError(sockfd);
        LOG_ERROR("Connector::handleError SO_ERROR = %d", err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    ::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        LOG_INFO("Connector::retry - Retry connecting to %s in %d milliseconds. ",
                 serverAddr_.toIpPort().c_str(), retryDelayMs_);
        // loop_->runAfter(retryDelayMs_/1000.0,
        //                 std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);

    }
    else
    {
        LOG_DEBUG("do not connect ");
    }
}   