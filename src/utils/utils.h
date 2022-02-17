#ifndef __SHANNON_NET_UTILS_H__
#define __SHANNON_NET_UTILS_H__

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "src/config/config.h"
#include "src/utils/redis.h"

namespace shannonnet {
pid_t GetThreadId();
std::time_t getTimeStamp();
std::tm *getTm(std::time_t timestamp);
std::string getCurTime(bool milliseconds = false);
std::vector<std::pair<std::string, uint>> readFileSize(const std::string &dir, const uint16_t serverNodeId,
                                                       const uint16_t clientNodeId);
std::string genRandomStr(uint16_t len = shannonnet::SAVE_INDEX_LEN);
uint32_t genRandomNum();
}  // namespace shannonnet
#endif  // __SHANNON_NET_UTILS_H__