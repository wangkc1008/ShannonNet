#include <map>
#include <string>
#include <vector>

#include "boost/format.hpp"

#include "src/LWE.hpp"
#include "src/common.h"
#include "src/utils/path.h"

void run() {
  time_t timeStamp = time(nullptr);
  std::vector<std::string> initSecrets{"secret_1.bin", "secret_2.bin"};
  std::vector<std::string> initName{shannonnet::SERVER_NAME, shannonnet::CLIENT_NAME};
  std::map<std::uint16_t, std::uint16_t> nodeIdMap = {{shannonnet::SERVER_NODE, shannonnet::CLIENT_NODE}};

  LOG(INFO) << "********************************Path & LWE********************************";
  boost::format nodeFmt("node_%d_%d");
  nodeFmt % shannonnet::SERVER_NODE % shannonnet::CLIENT_NODE;
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr(new shannonnet::LWE<shannonnet::S_Type>());
  std::vector<std::vector<std::string>> secretPathVec;
  for (auto &name : initName) {
    // .../shannonnet/%s/secrets/node_%d_%d/
    boost::format secretSavePathFmt(shannonnet::SECRET_SAVE_PATH);
    secretSavePathFmt % name;
    shannonnet::Path secretSavePath(secretSavePathFmt.str());
    secretSavePath = secretSavePath / nodeFmt.str();
    if (!secretSavePath.Exists()) {
      LOG_IF(WARNING, !secretSavePath.CreateDirectories())
        << "Init create directories warning, path: " << secretSavePath.ToString();
    }
    LOG(INFO) << "Init create directories success, path: " << secretSavePath.ToString();

    std::vector<std::string> itemSecretPathVec;
    for (auto &secretFile : initSecrets) {
      auto secretFilePath = (secretSavePath / secretFile).ToString();
      if (name == shannonnet::SERVER_NAME) {
        lwePtr->generateSecretFile(secretFilePath);
        LOG(INFO) << "Init generate secret success, file: " << secretFilePath;
      }
      itemSecretPathVec.emplace_back(std::move(secretFilePath));
    }
    secretPathVec.emplace_back(std::move(itemSecretPathVec));

    // .../shannonnet/%s/running/
    boost::format runningSavePathFmt(shannonnet::RUNNING_SAVE_PATH);
    runningSavePathFmt % name;
    shannonnet::Path runningSavePath(runningSavePathFmt.str());
    if (!runningSavePath.Exists()) {
      LOG_IF(WARNING, !runningSavePath.CreateDirectories())
        << "Init create directories warning, path: " << runningSavePath.ToString();
    }
    LOG(INFO) << "Init create directories success, path: " << runningSavePath.ToString();

    // .../shannonnet/%s/waiting/
    boost::format waitingSavePathFmt(shannonnet::WAITING_SAVE_PATH);
    waitingSavePathFmt % name;
    shannonnet::Path waitingSavePath(waitingSavePathFmt.str());
    if (!waitingSavePath.Exists()) {
      LOG_IF(WARNING, !waitingSavePath.CreateDirectories())
        << "Init create directories warning, path: " << waitingSavePath.ToString();
    }
    LOG(INFO) << "Init create directories success, path: " << waitingSavePath.ToString();
  }

  uint32_t secretASize = SN_DEBUG ? shannonnet::DEBUG_SECRET_A_SIZE : shannonnet::SECRET_A_SIZE;
  for (size_t i = 0; i < secretPathVec.size(); ++i) {
    for (size_t j = 0; j < secretPathVec[0].size(); j += 2) {
      std::vector<char> bufferVec(secretASize);
      std::ifstream ifs(secretPathVec[j][i], std::ios::in | std::ios::binary);
      ifs.read(bufferVec.data(), bufferVec.size());
      ifs.close();

      std::ofstream ofs(secretPathVec[j + 1][i], std::ios::out | std::ios::binary | std::ios::ate);
      ofs.write(bufferVec.data(), bufferVec.size());
      ofs.flush();
      ofs.close();
      LOG(INFO) << "Init copy secret success, before: " << secretPathVec[j][i]
                << ", after: " << secretPathVec[j + 1][i];
    }
  }

  LOG(INFO) << "********************************Redis********************************";
  auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());
  redis.del(shannonnet::KEY_GENERATE_SECRET_LIST);
  LOG(INFO) << "Init del key success, key: " << shannonnet::KEY_GENERATE_SECRET_LIST;

  for (auto &name : initName) {
    auto keyIndexTime = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % name).str();
    redis.del(keyIndexTime);
    LOG(INFO) << "Init del key success, key: " << keyIndexTime;
    for (auto &secretFile : initSecrets) {
      LOG_IF(FATAL, !redis.zadd(keyIndexTime, secretFile.substr(0, secretFile.find_last_of(".")), timeStamp));
      LOG(INFO) << "Init zadd key success, key: " << keyIndexTime << ", member: " << secretFile
                << ", score: " << timeStamp;
    }
    auto keyIndexCount = (boost::format(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET) % name).str();
    redis.del(keyIndexCount);
    LOG(INFO) << "Init del key success, key: " << keyIndexCount;
  }

  for (auto &item : nodeIdMap) {
    boost::format keyIndexValidFmt(shannonnet::KEY_NODE_INDEX_VALID_ZSET);
    keyIndexValidFmt % item.first % item.second;
    redis.del(keyIndexValidFmt.str());
    LOG(INFO) << "Init del key success, key: " << keyIndexValidFmt.str();

    for (auto &secretFile : initSecrets) {
      redis.zadd(keyIndexValidFmt.str(), secretFile.substr(0, secretFile.find_last_of(".")),
                 static_cast<uint16_t>(shannonnet::SECRET_STATUS::NODE_SECRET_VALID));
    }
    LOG(INFO) << "Init zadd key success, key: " << keyIndexValidFmt.str();
  }
}

int main() {
  shannonnet::InitLog(shannonnet::LOG_NAME_INIT);
  run();
  return 0;
}