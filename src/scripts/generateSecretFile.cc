#include <algorithm>
#include <bitset>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

#include "boost/format.hpp"
#include "crc32c/crc32c.h"
#include "nlohmann/json.hpp"
#include "src/common.h"

#include "src/LWE.hpp"

namespace shannonnet {
/**
 * @brief
 *    生成随机数量的秘钥文件
 *
 * @param pathAndSizeVec
 * @return decltype(pathAndSizeVec)
 */
auto generateRandomFile(std::vector<std::pair<std::string, uint>> pathAndSizeVec) -> decltype(pathAndSizeVec) {
  decltype(pathAndSizeVec) newPathAndSizeVec;
  uint size = pathAndSizeVec.size();
  std::unordered_map<uint, uint> randomUMap;
  for (uint i = 0; i < FILE_NUM; ++i) {
    while (true) {
      uint random = rand() % size;
      if (randomUMap.find(random) != randomUMap.end()) {
        continue;
      }
      newPathAndSizeVec.emplace_back(pathAndSizeVec[random]);
      randomUMap[random] = 1;
      break;
    }
  }
  return newPathAndSizeVec;
}

/**
 * @brief
 *    生成随机数 生成秘钥时作为offset
 * @param fileNum
 * @return std::vector<std::vector<uint>>
 */
std::vector<std::vector<uint>> generateRandom(uint32_t fileNum) {
  std::vector<std::vector<uint>> vec;
  vec.reserve(fileNum * THREAD_NUM);
  auto num = SECRET_FILE_SIZE / (READ_NUM / 8) / THREAD_NUM;
  for (size_t i = 0; i < fileNum * THREAD_NUM; ++i) {
    std::vector<uint> subVec;
    subVec.reserve(num);
    for (size_t j = 0; j < num; ++j) {
      auto random = rand() % SECRET_A_SIZE;
      if ((random + READ_LEN) > SECRET_A_SIZE) {
        random = SECRET_A_SIZE - READ_LEN;
      }
      subVec.emplace_back(std::move(random));
    }
    vec.emplace_back(std::move(subVec));
  }
  return vec;
}

/**
 * @brief
 *    为每个秘钥文件生成固定数量tmp秘钥文件 每个tmp秘钥大小等于 SECRET_FILE_SIZE / THREAD_NUM
 *    累计生成 THREAD_NUM * fileNum 个tmp秘钥文件
 *          461cdscnc515asik_0_0.bin
 *          461cdscnc515asik_0_1.bin
 *          461cdscnc515asik_1_0.bin
 *          461cdscnc515asik_1_1.bin
 *          ......
 * @param path
 * @param fileIdx
 * @param threadIdx
 * @param vec
 * @param saveIdx
 * @param source
 */
void processTmpSecret(const std::string &path, uint fileIdx, uint threadIdx, const std::vector<uint> &vec,
                      const std::string &saveIdx, const std::string &source) {
  auto logMsgFmt = (boost::format("secretFile => %s; fileId => %d; threadId => %d; randomNumSize => %d") % path %
                    fileIdx % threadIdx % vec.size())
                     .str();
  LOG(INFO) << "Executing processTmpSecret, msg: " + logMsgFmt;
  std::ifstream ifs(path, std::ios::in | std::ios::binary);
  if (!ifs.is_open()) {
    LOG(ERROR) << "File open failed, file: " + path;
    return;
  }
  // 将秘钥文件等分给每个线程处理
  uint bufferSize = THREAD_NUM > 1 ? (SECRET_A_SIZE / THREAD_NUM) : SECRET_A_SIZE;
  std::vector<char> bufVec(bufferSize);
  ifs.seekg(bufferSize * threadIdx, std::ios::beg);
  ifs.read(bufVec.data(), static_cast<std::streamsize>(bufVec.size()));
  ifs.close();

  // 根据随机数确定offset 每 128bit 生成 1bit 的秘钥
  // 生成与所需秘钥大小相同的tmp秘钥
  uint16_t num = 0;
  std::stringstream ss;
  for (auto &random : vec) {
    num = 0;
    std::stringstream charSS;
    for (size_t i = 0; i < READ_NUM; ++i) {
      uint32_t offset = (random * i) % bufferSize;
      std::string str{bufVec.data() + offset, READ_LEN / 8};
      std::bitset<8> bitFirst8(0);
      for (auto &c : str) {
        bitFirst8 ^= std::bitset<8>(c);
      }
      std::string bitStr = bitFirst8.to_string();
      std::bitset<1> bitFirst1(0);
      for (auto &cc : bitStr) {
        bitFirst1 ^= std::bitset<1>(cc);
      }
      charSS << bitFirst1.to_string();
      if (++num % 8 == 0) {
        std::bitset<8> bits(charSS.str());
        ss << (char)bits.to_ulong();
        charSS.str("");
      }
    }
  }

  // running/saveIdx/tmp目录是否存在
  auto runningSavePathFmt = (boost::format(RUNNING_SAVE_PATH) % source).str();
  Path runningTmpPath(runningSavePathFmt);
  runningTmpPath = runningTmpPath / saveIdx / std::string("tmp");
  if (!runningTmpPath.IsDirectory()) {
    LOG(ERROR) << "Dir not exists, dir: " + runningTmpPath.ToString();
    return;
  }

  // 生成的秘钥写入tmp目录
  auto fileThreadFmt = (boost::format("%s_%d_%d.bin") % saveIdx % fileIdx % threadIdx).str();
  std::string runningTmpFile = (runningTmpPath / fileThreadFmt).ToString();
  std::ofstream ofs(runningTmpFile, std::ios::out | std::ios::binary | std::ios::ate);
  if (!ofs.is_open()) {
    LOG(ERROR) << "File open failed: " + runningTmpFile;
    return;
  }
  ofs << ss.str();
  ofs.flush();
  ofs.close();
  LOG(INFO) << "Execute processTmpSecret successfully, msg: " + logMsgFmt +
                 ", writeSize: " + std::to_string(ss.str().size());
}

/**
 * @brief
 *    根据线程id将n个秘钥文件生成的n个tmp秘钥 异或 合并为1个秘钥文件
 *    累计生成 THREAD_NUM 个秘钥文件 每个文件大小等于所需秘钥大小的 1 / THREAD_NUM
 *        461cdscnc515asik_0_0.bin |
 *        461cdscnc515asik_1_0.bin | => 461cdscnc515asik_0.bin
 *
 *        461cdscnc515asik_0_1.bin |
 *        461cdscnc515asik_1_1.bin | => 461cdscnc515asik_1.bin
 * @param threadIdx
 * @param fileNum
 * @param saveIdx
 * @param source
 */
void processBinSecret(uint threadIdx, uint fileNum, const std::string &saveIdx, const std::string &source) {
  auto logMsgFmt =
    (boost::format("fileNum => %d; threadId => %d; saveIdx => %s") % fileNum % threadIdx % saveIdx).str();
  LOG(INFO) << "Executing processBinSecret, msg: " + logMsgFmt;

  auto runningSavePathFmt = (boost::format(RUNNING_SAVE_PATH) % source).str();
  Path runningSavePath(runningSavePathFmt);
  runningSavePath = runningSavePath / saveIdx;

  auto runningTmpPath = runningSavePath / std::string("tmp");
  std::vector<std::vector<char>> bufferVec;

  // 读取tmp下秘钥文件
  for (size_t i = 0; i < fileNum; ++i) {
    auto subSecretPath = runningTmpPath / (boost::format("%s_%d_%d.bin") % saveIdx % i % threadIdx).str();
    if (!subSecretPath.Exists()) {
      LOG(ERROR) << "File not exists, file: " + runningTmpPath.ToString();
      continue;
    }

    std::ifstream ifs(subSecretPath.ToString(), std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
      LOG(ERROR) << "File open failed, file: " + subSecretPath.ToString();
      continue;
    }
    ifs.seekg(0, std::ios::end);
    auto fileSize = ifs.tellg();
    std::vector<char> subBufferVec(fileSize);
    ifs.seekg(0, std::ios::beg);
    ifs.read(subBufferVec.data(), fileSize);
    bufferVec.emplace_back(std::move(subBufferVec));
    // 删掉临时文件
    LOG_IF(ERROR, !subSecretPath.Remove()) << "Failed to delete file, file: " + subSecretPath.ToString();
    LOG(INFO) << "Delete file successfully, file: " + subSecretPath.ToString();
  }

  // 对应同一线程id不同文件id的秘钥进行 异或 得到线程id标注的秘钥文件
  std::stringstream ss;
  for (size_t i = 0; i < bufferVec[0].size(); ++i) {
    std::bitset<8> bitFirst8(0);
    for (size_t j = 0; j < fileNum; ++j) {
      bitFirst8 ^= std::bitset<8>(bufferVec[j][i]);
    }
    ss << (char)bitFirst8.to_ulong();
  }

  // 秘钥文件写入
  std::string saveFile = (runningSavePath / (boost::format("%s_%d.bin") % saveIdx % threadIdx).str()).ToString();
  std::ofstream ofs(saveFile, std::ios::out | std::ios::binary | std::ios::ate);
  if (!ofs.is_open()) {
    LOG(ERROR) << "File open failed: " + saveFile;
    return;
  }
  ofs << ss.str();
  ofs.flush();
  ofs.close();
  LOG(INFO) << "Execute processBinSecret successfully, msg: " + logMsgFmt +
                 ", writeSize: " + std::to_string(ss.str().size());
}

/**
 * @brief
 *    生成秘钥文件脚本
 *
 */
void generateSecretFileScript() {
  LOG(INFO) << "PID: " << getpid();
  auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr(new shannonnet::LWE<shannonnet::S_Type>());
  std::stringstream ss;
  std::string source;
  std::string saveIdx;
  uint32_t randomNum = 0;
  uint16_t serverNodeId = 0;
  uint16_t clientNodeId = 0;
  while (true) {
    try {
      // 接收消息队列
      auto redisVal = redis.rpop(KEY_GENERATE_SECRET_LIST);
      if (!redisVal) {
        sleep(5);
        LOG(INFO) << "No message.";
        continue;
      }

      auto jsonVal = nlohmann::json::parse(*redisVal);
      for (auto &item : jsonVal.items()) {
        ss << "\n\t" << item.key() << "->" << item.value();
      }
      if (jsonVal.find("source") == jsonVal.end() || jsonVal.find("randomNum") == jsonVal.end() ||
          jsonVal.find("index") == jsonVal.end() || jsonVal.find("serverNodeId") == jsonVal.end() ||
          jsonVal.find("clientNodeId") == jsonVal.end()) {
        LOG(WARNING) << "No index or serverNodeId or clientNodeId in msg, got" + ss.str();
        sleep(5);
        continue;
      }
      LOG(INFO) << "Got json msg, msg: " + ss.str();
      ss.str("");

      source = jsonVal["source"];
      randomNum = jsonVal["randomNum"];
      saveIdx = jsonVal["index"];
      serverNodeId = jsonVal["serverNodeId"];
      clientNodeId = jsonVal["clientNodeId"];

      // 当前index对应的秘钥是否生成
      auto keyIndexTimefmt = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % source).str();
      auto createTime = redis.zscore(keyIndexTimefmt, saveIdx);
      if (createTime) {
        LOG(ERROR) << "Secrets corresponding to current index have been generated, saveIdx: " + saveIdx;
        sleep(5);
        continue;
      }

      // secrets下node_serverNodeId_cientNodeId目录是否存在
      Path secretSavePath((boost::format(SECRET_SAVE_PATH) % source).str());
      secretSavePath = secretSavePath / (boost::format("node_%d_%d") % serverNodeId % clientNodeId).str();
      if (!secretSavePath.Exists()) {
        LOG(ERROR) << "Dir not exists, dir: " + secretSavePath.ToString();
        continue;
      }

      // 创建running/saveIdx目录
      Path runningSavePath((boost::format(RUNNING_SAVE_PATH) % source).str());
      runningSavePath = runningSavePath / saveIdx;
      if (!runningSavePath.Exists()) {
        if (!runningSavePath.CreateDirectory()) {
          LOG(ERROR) << "Create dir failed, dir: " + runningSavePath.ToString();
          continue;
        }
      }
      LOG(INFO) << "Dir created successfuly, dir: " + runningSavePath.ToString();

      // 创建running/saveIdx/tmp目录
      auto runningTmpPath = runningSavePath / std::string("tmp");
      if (!runningTmpPath.Exists()) {
        if (!runningTmpPath.CreateDirectory()) {
          LOG(ERROR) << "Create dir failed, dir: " + runningTmpPath.ToString();
          continue;
        }
      }
      LOG(INFO) << "Dir created successfuly, dir: " + runningTmpPath.ToString();

      // 创建waiting/saveIdx目录
      Path waitingSavePath((boost::format(WAITING_SAVE_PATH) % source).str());
      waitingSavePath = waitingSavePath / saveIdx;
      if (!waitingSavePath.Exists()) {
        if (!waitingSavePath.CreateDirectory()) {
          LOG(ERROR) << "Create dir failed, dir: " + waitingSavePath.ToString();
          continue;
        }
      }
      LOG(INFO) << "Dir created successfuly, dir: " + waitingSavePath.ToString();

      // 将json中的随机数设为随机种子
      srand(randomNum);

      // 读取secrets下node_serverNodeId_cientNodeId目录中所有可用的文件路径和大小
      auto pathAndSizeVec = shannonnet::readFileSize(secretSavePath.ToString(), serverNodeId, clientNodeId);
      // 秘钥文件数量超过设定的数量时随机抽取部分文件
      if (pathAndSizeVec.size() > FILE_NUM) {
        pathAndSizeVec = generateRandomFile(pathAndSizeVec);
      }

      // 设定随机数
      uint pathAndSizeVecSize = pathAndSizeVec.size();
      auto randomVec = generateRandom(pathAndSizeVecSize);
      // 为每个秘钥文件生成固定数量tmp秘钥文件
      std::vector<std::thread> threads;
      for (size_t fileIdx = 0; fileIdx < pathAndSizeVecSize; ++fileIdx) {
        for (size_t threadIdx = 0; threadIdx < THREAD_NUM; ++threadIdx) {
          threads.emplace_back(processTmpSecret, pathAndSizeVec[fileIdx].first, fileIdx, threadIdx,
                               randomVec[fileIdx * THREAD_NUM + threadIdx], saveIdx, source);
        }
      }
      for (auto &thread : threads) {
        thread.join();
      }
      LOG(INFO) << "Execute processTmpSecret successfuly, saveIdx: " + saveIdx;

      // 根据线程id将n个秘钥文件生成的n个tmp秘钥 异或 合并为1个秘钥文件
      std::vector<std::thread> threadsMerge;
      for (size_t threadIdx = 0; threadIdx < THREAD_NUM; ++threadIdx) {
        threadsMerge.emplace_back(processBinSecret, threadIdx, pathAndSizeVecSize, saveIdx, source);
      }
      for (auto &threadMerge : threadsMerge) {
        threadMerge.join();
      }
      LOG(INFO) << "Execute processBinSecret successfuly, saveIdx: " + saveIdx;
      LOG_IF(ERROR, !runningTmpPath.Remove()) << "Failed to delete dir, dir: " + runningTmpPath.ToString();

      // 将 THREAD_NUM 个秘钥文件合并为 1 个秘钥文件
      // 461cdscnc515asik_0.bin |
      // 461cdscnc515asik_1.bin | => 461cdscnc515asik.bin
      std::vector<char> bufferVec(SECRET_FILE_SIZE);
      for (size_t i = 0; i < THREAD_NUM; ++i) {
        auto subSecretPath = runningSavePath / (saveIdx + "_" + std::to_string(i) + ".bin");
        std::ifstream ifs(subSecretPath.ToString(), std::ios::in | std::ios::binary);
        if (!ifs.is_open()) {
          LOG(ERROR) << "File open failed, file: " + subSecretPath.ToString();
          continue;
        }
        ifs.seekg(0, std::ios::end);
        uint fileSize = ifs.tellg();

        ifs.seekg(0, std::ios::beg);
        ifs.read(bufferVec.data() + i * (SECRET_FILE_SIZE / THREAD_NUM), fileSize);
        ifs.close();
        // 删掉临时文件
        LOG_IF(ERROR, !subSecretPath.Remove()) << "Failed to delete file, file: " + subSecretPath.ToString();
        LOG(INFO) << "Delete file successfuly, file: " + subSecretPath.ToString();
      }
      // 计算生成running秘钥文件的crc32校验值
      auto crc32Val = crc32c::Crc32c(bufferVec.data(), bufferVec.size());
      LOG(INFO) << "Execute running secret crc32 successfuly, crc32c: " + std::to_string(crc32Val) +
                     ", saveIdx: " + saveIdx;
      std::string saveFile = (runningSavePath / (saveIdx + ".bin")).ToString();
      std::ofstream ofs(saveFile, std::ios::out | std::ios::binary);
      if (!ofs.is_open()) {
        LOG(ERROR) << "File open failed: " + saveFile;
        continue;
      }
      ofs.write(bufferVec.data(), bufferVec.size());
      ofs.flush();
      ofs.close();
      LOG(INFO) << "Save running secret successfully, saveFile: " + saveFile;
      // 只有来自server的消息才会生成waiting秘钥
      // 写redis
      // zadd index => create_time
      // zadd index => progress
      // del index
      if (source == shannonnet::SERVER_NAME) {
        auto waitingSaveFile = (waitingSavePath / (saveIdx + "_waited.bin")).ToString();
        lwePtr->generateSecretFile(waitingSaveFile, true);
        LOG(INFO) << "Save waiting secret successfully, saveFile: " + waitingSaveFile;
      }
      time_t timeStamp = time(nullptr);
      auto score_time = redis.zadd(keyIndexTimefmt, saveIdx, timeStamp);
      if (!score_time) {
        LOG(ERROR) << "Redis zadd failed, saveIdx: " + saveIdx + ", score_time: " + std::to_string(score_time);
        continue;
      }
      LOG(INFO) << "Redis zadd success, saveIdx: " + saveIdx + ", score_time: " + std::to_string(score_time);
      if (source == shannonnet::SERVER_NAME) {
        redis.zadd(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET, saveIdx, 0);
        LOG_IF(ERROR, !redis.del(saveIdx)) << "Redis del failed, saveIdx: " + saveIdx;
      }
    } catch (const sw::redis::Error &e) {
      LOG(ERROR) << e.what();
    }
  }
}
}  // namespace shannonnet

int main(int argc, char **argv) {
  shannonnet::InitLog(shannonnet::LOG_GEN_SECRET);
  shannonnet::generateSecretFileScript();
  return 0;
}
