#include "Buffer.h"

#include "errno.h"
#include "sys/uio.h"
#include "unistd.h"

/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的 但是从fd上读取数据的时候， 却不知道tcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd,  int* saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间   64k
    
    // struct iovec存放了两个成员：iov_base:地址空间的首地址 iov_len：长度
    struct iovec vec[2];

    const size_t writalbe = writableBytes();    // 这是Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writalbe;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // iovcnt 是 iov 数组中 iovec 结构的数量
    //如果Buffer剩余的可写缓冲区小于extrabuf的大小则有两块缓冲区
    const int iovcnt = (writalbe < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt); //readv可以从文件描述符中读取数据到多个缓冲区中
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writalbe) // Buffer的可写缓冲区已经够存储读取的数据
    {
        writerIndex_ += n;
    }
    else //表示 extrabuf 中也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writalbe); // writerIndex_开始写入 n - writalbe大小的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}