#ifndef __SHANNON_NET_UTILS_H__
#define __SHANNON_NET_UTILS_H__

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace shannonnet {
std::time_t getTimeStamp();
std::tm *getTm(std::time_t timestamp);
std::string getCurTime(bool milliseconds = false);
}  // namespace shannonnet
#endif  // __SHANNON_NET_UTILS_H__