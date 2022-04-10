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

#define TIMERSTART(label)                                                \
  std::chrono::time_point<std::chrono::system_clock> a##label, b##label; \
  a##label = std::chrono::system_clock::now();

#define TIMERSTOP(label)                                            \
  b##label = std::chrono::system_clock::now();                      \
  std::chrono::duration<double> delta##label = b##label - a##label; \
  std::cout << "# elapsed time (" << #label << "): " << delta##label.count() << "s" << std::endl;

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