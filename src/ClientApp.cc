#include <future>

#include "boost/format.hpp"
#include "crc32c/crc32c.h"
#include "nlohmann/json.hpp"
#include <oatpp/web/client/HttpRequestExecutor.hpp>

#include "src/ClientComponent.hpp"
#include "src/main/Decrypt.h"
#include "src/app/ClientApi.hpp"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
void clientRun() {
  LOG(INFO) << "Client nodeId is " << CLIENT_NODE << ".";

  shannonnet::ClientComponent components;
  auto requestExecutor =
    oatpp::web::client::HttpRequestExecutor::createShared(components.clientConnectionProvider.getObject());
  auto jsonObjectMapper = components.objectMapper.getObject();
  auto client = ClientApi::createShared(requestExecutor, jsonObjectMapper);
  auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

  // Dto
  uint16_t serverNodeId = 0;
  uint32_t randomNum = 0;
  std::string index;
  // json
  nlohmann::json jsonData = {};
  // redis
  auto serverIndexTime = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % SERVER_NAME).str();
  auto clientIndexTime = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % CLIENT_NAME).str();
  auto clientIndexCount = (boost::format(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET) % CLIENT_NAME).str();
  // file
  auto runningSavePathFmt = (boost::format(RUNNING_SAVE_PATH) % CLIENT_NAME).str();
  auto waitingSavePathFmt = (boost::format(WAITING_SAVE_PATH) % CLIENT_NAME).str();
  auto secretSavePathFmt = (boost::format(SECRET_SAVE_PATH) % CLIENT_NAME).str();
  // dto
  auto genSecretPost = GenSecretPostDto::createShared();
  while (true) {
    // 判断是否要更新秘钥
    // todo
    // 调预生成秘钥接口
    auto genSecretResponse = client->genSecret(genSecretPost);
    auto genSecretMsg =
      genSecretResponse->readBodyToDto<oatpp::Object<shannonnet::MessageGenSecretDto>>(jsonObjectMapper);
    auto jsonMapperData = jsonObjectMapper->writeToString(genSecretMsg);
    if (genSecretMsg->statusCode != 200) {
      LOG(ERROR) << "Api genSecret result error, msg: " << jsonMapperData->c_str();
      sleep(10);
      continue;
    }
    LOG(INFO) << "Api genSecret result success, msg: " << jsonMapperData->c_str();

    auto genSecretDto = genSecretMsg->data;
    serverNodeId = genSecretDto->nodeId;
    randomNum = genSecretDto->randomNum;
    index = genSecretDto->index;
    serverNodeId = 0;

    // json消息
    jsonData = {{"source", CLIENT_NAME},        {"randomNum", randomNum},      {"index", index},
                {"serverNodeId", serverNodeId}, {"clientNodeId", CLIENT_NODE}, {"value", ""}};
    LOG(INFO) << "Api client json data, jsonData: " << jsonData.dump();

    // 队列通知生成client秘钥
    auto resLPush = redis.lpush(shannonnet::KEY_GENERATE_SECRET_LIST, jsonData.dump());
    if (!resLPush) {
      LOG(ERROR) << "Api client json lpush error";
      sleep(10);
      continue;
    }

    // 循环判断server和client秘钥是否生成完毕
    // 秘钥file
    Path runningSavePath(runningSavePathFmt);
    runningSavePath = runningSavePath / index / index + ".bin";
    while (true) {
      uint32_t serverCreateTime = redis.zscore(serverIndexTime, index).value();
      uint32_t clientCreateTime = redis.zscore(clientIndexTime, index).value();
      if (serverCreateTime && clientCreateTime) {
        LOG(INFO) << "Api client secret generated over, serverCreateTime: " << serverCreateTime
                  << ", clientCreateTime: " << clientCreateTime;
        if (!runningSavePath.Exists()) {
          LOG(ERROR) << "Api client running secret not exist, runningSavePath: " << runningSavePath.ToString()
                     << ", json: " << jsonMapperData->c_str();
          sleep(10);
          continue;
        }
        break;
      }
      LOG(WARNING) << "Api client secret not generate over, serverCreateTime: " << serverCreateTime
                   << ", clientCreateTime: " << clientCreateTime;
      sleep(10);
    }
    // 格式化进度消息
    auto logFmt =
      (boost::format("serverNodeId => %u, clientNodeId => %u, index => %s") % serverNodeId % CLIENT_NODE % index).str();

    // 存储接收到的秘钥文件
    Path waitingSavePath(waitingSavePathFmt);
    waitingSavePath = waitingSavePath / index;
    Path secretSavePath(secretSavePathFmt);
    auto nodeFmt = (boost::format("node_%u_%u") % serverNodeId % CLIENT_NODE).str();
    secretSavePath = secretSavePath / nodeFmt / index + ".bin";

    // 循环拉秘钥
    bool runningFlag;
    uint32_t progress = 0;
    shannonnet::Decrypt::ptr decrypt(new shannonnet::Decrypt(client, jsonObjectMapper, index, logFmt,
                                                             runningSavePath.ToString(), waitingSavePath.ToString(),
                                                             secretSavePath.ToString()));
    while (true) {
      auto startTime = std::chrono::system_clock::now();
      std::vector<std::future<bool>> clientAsyncVec;
      for (size_t clientThreadId = 0; clientThreadId < CLIENT_THREAD_NUM; ++clientThreadId) {
        progress = redis.zincrby(clientIndexCount, 1, index);
        if (progress <= PROGRESS) {
          clientAsyncVec.emplace_back(
            std::async(std::launch::async, &shannonnet::Decrypt::process, decrypt, progress - 1));
        }
        if (progress >= PROGRESS) {
          break;
        }
      }

      for (auto &futureItem : clientAsyncVec) {
        runningFlag = futureItem.get();
        if (!runningFlag) {
          break;
        }
      }

      // 判断是否接收完成
      if (progress >= PROGRESS) {
        runningFlag = decrypt->gather(progress);
        if (runningFlag) {
          break;
        }
      }
      LOG(WARNING) << "Time: " << (std::chrono::system_clock::now() - startTime).count();
      // 当前秘钥未接收完毕
    }
    // 处理完一个秘钥文件
    sleep(60);
  }
}
}  // namespace shannonnet

int main(int argc, char **argv) {
  shannonnet::InitLog(shannonnet::LOG_NAME_CLIENT);
  oatpp::base::Environment::init();
  shannonnet::clientRun();
  std::cout << "\nEnvironment:\n";
  std::cout << "objectsCount = " << oatpp::base::Environment::getObjectsCount() << "\n";
  std::cout << "objectsCreated = " << oatpp::base::Environment::getObjectsCreated() << "\n\n";
  oatpp::base::Environment::destroy();
  return 0;
}