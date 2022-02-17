#include "boost/format.hpp"
#include <sys/syscall.h>

#include "src/utils/log.h"
#include "src/utils/path.h"

#include "src/utils/utils.h"

namespace shannonnet {
pid_t GetThreadId() { return syscall(SYS_gettid); }

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

std::vector<std::pair<std::string, uint32_t>> readFileSize(const std::string &dir, const uint16_t serverNodeId,
                                                           const uint16_t clientNodeId) {
  Path dirPath(dir);
  std::vector<std::pair<std::string, uint32_t>> pathAndSizeVec;
  if (!dirPath.IsDirectory()) {
    LOG(ERROR) << "Invalid path, dir must be directory but got: " << dir << ".";
    return pathAndSizeVec;
  }

  std::shared_ptr<Path::DirIterator> dIt = Path::DirIterator::OpenDirectory(&dirPath);
  if (dIt == nullptr) {
    LOG(ERROR) << "Invalid path, failed to open directory: " << dirPath.ToString() << ".";
    return pathAndSizeVec;
  }
  auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());
  auto redisKeyFmt = (boost::format(shannonnet::KEY_NODE_INDEX_VALID_ZSET) % serverNodeId % clientNodeId).str();

  while (dIt->HasNext()) {
    try {
      Path secretFilePath = dIt->Next();
      if (secretFilePath.IsDirectory()) {
        continue;
      }

      if (secretFilePath.Extension() != ".bin") {
        continue;
      }

      uint16_t isValid = redis.zscore(redisKeyFmt, secretFilePath.Basename()).value();
      if (isValid != shannonnet::NODE_SECRET_VALID) {
        LOG(WARNING) << "readFileSize secret is invalid, secret: " + secretFilePath.ToString();
        continue;
      }

      std::ifstream ifs(secretFilePath.ToString(), std::ios::in | std::ios::binary);
      ifs.seekg(0, std::ios::end);
      uint64_t size = ifs.tellg();
      pathAndSizeVec.emplace_back(std::make_pair(secretFilePath.ToString(), size));
    } catch (const std::exception &err) {
      LOG(ERROR) << "Invalid path, failed to read secret file: " << dirPath.ToString();
      return pathAndSizeVec;
    }
  }
  return pathAndSizeVec;
}

std::string genRandomStr(uint16_t len) {
  char randomStr[len + 1];
  srand(time(nullptr));
  for (uint16_t i = 0; i < len; ++i) {
    auto random = rand();
    switch (random % 3) {
    case 1:
      randomStr[i] = 'A' + rand() % 26;
      break;
    case 2:
      randomStr[i] = 'a' + rand() % 26;
      break;
    default:
      randomStr[i] = '0' + rand() % 10;
      break;
    }
  }
  randomStr[len] = '\0';
  return randomStr;
}

uint32_t genRandomNum() {
  srand(time(nullptr));
  return rand();
}

}  // namespace shannonnet