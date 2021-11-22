#include "src/utils/utils.h"

namespace shannonnet {
std::time_t getTimeStamp() {
  std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp =
    std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
  auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
  std::time_t timestamp = tmp.count();
  return timestamp;
}

std::tm *getTm(std::time_t timestamp) {
  timestamp += (std::time_t)8 * 60 * 60 * 1000;
  auto tp =
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(std::chrono::milliseconds(timestamp));
  auto tt = std::chrono::system_clock::to_time_t(tp);
  std::tm *now = std::gmtime(&tt);
  return now;
}

std::string getCurTime(bool milliseconds) {
  auto timestamp = getTimeStamp();
  std::tm *tm = getTm(timestamp);
  std::stringstream ss;
  ss << (tm->tm_year + 1900) << (tm->tm_mon + 1) << tm->tm_mday << "_" << std::setw(2) << std::setfill('0')
     << tm->tm_hour << std::setw(2) << std::setfill('0') << tm->tm_min << std::setw(2) << std::setfill('0')
     << tm->tm_sec;
  if (milliseconds) {
    ss << "_" << std::to_string(timestamp).substr(10);
  }
  return ss.str();
}
}  // namespace shannonnet