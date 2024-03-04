#pragma once

/*
 * nocncopyable被继承后，派生类对象可以正常的构造和析构， 但是派生类对象
 * 无法进行拷贝和赋值操作
 */
class noncopyable
{
public:
	noncopyable(const noncopyable&) = delete;
	noncopyable& operator=(const noncopyable&) = delete;
protected:
	noncopyable() = default;
	~noncopyable() = default;
};
