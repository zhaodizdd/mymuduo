#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲区类型的定义
/**
 * Buffer的结构： 
 * +---------------------+-----------------+----------------+
 * |  prependable bytes  | readalbe bytes  | writable bytes |
 * |                     |                 |                |
 * +---------------------+-----------------+----------------+
 * |                     |                 |                |
 * 0         <=     readIndex    <=    writerIndex   <=     size
*/
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;  // Buffer的头部长度
    static const size_t kInitalSize = 1024; // Buffer的读写部分长度初始大小

    explicit Buffer(size_t initialSize = kInitalSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    // 可读的数据长度
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    // 可写的数据长度
    size_t writableBytes() const 
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区可读数据的的起始地址
    const char* peek() const 
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    // 对已经读取处理的缓冲区进行复位操作
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 应用只读取了刻度缓冲区数据的一部分， 就是len 还剩readerIndex_ += len 到 writerIndex_
        }
        else    // len == readableBytes()
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据， 转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAllAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAllAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);  // 上面的语句把缓存区可读的数据， 已经读取处理， 这里需要对缓冲区进行复位操作
        return result;
    }

    // bufer_.size() - writerIndex_   len
    // 确定读取数据，如果数据len过大就扩容
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len) // 写的数据大于可写数据的长度
        {
            makeSpace(len);   // 扩容
        }
    }

    // 把[data, data + len]内存上的数据，添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }
    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin()
    {
        return &*buffer_.begin();   // 取vector底层数组首元素的地址，及数组的起始地址
    }
    const char* begin() const 
    {
        return &*buffer_.begin();
    }
    void makeSpace(size_t len)
    {
        // 可能存在读取了一部分后的空余空间，满足写的空间就不用扩容
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                    begin() + writerIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};