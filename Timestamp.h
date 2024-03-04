#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
	Timestamp();
	explicit Timestamp(int64_t microSecondsSinceEpoch);

	// 交换两个时间点
	void swap(Timestamp &that)
	{
		std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
	}

	// 返回该时间戳
	int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
	// 获取当前时间
	static Timestamp now();
	// 让时间清零
	static Timestamp invalid() { return Timestamp(); }
	// 返回时间的字符串形式 %4d%02d%02d %02d:%02d:%02d 20240301 21:26:21
	std::string toString() const;
	// 判断当前时间戳是否大于0
	bool valid() const { return microSecondsSinceEpoch_ > 0; }

	static const int kMicroSecondsPerSecond = 1000 * 1000; //秒和微妙的换算
private:
	int64_t microSecondsSinceEpoch_; // 时间戳
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

// 从当前时间加上对应秒数
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
	int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
	return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}