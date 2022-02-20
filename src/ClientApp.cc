#include "crc32c/crc32c.h"
#include <oatpp/web/client/HttpRequestExecutor.hpp>

#include "boost/format.hpp"
#include "nlohmann/json.hpp"

#include "ClientComponent.hpp"
#include "LWE.hpp"
#include "app/ClientApi.hpp"
#include "common.h"
#include "dto/DTOs.hpp"

namespace shannonnet {
void clientRun() {
  LOG(INFO) << "Client nodeId is " << CLIENT_NODE << ".";

  shannonnet::ClientComponent component;
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ClientConnectionProvider>, clientConnectionProvider);
  OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);
  auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(clientConnectionProvider);
  auto client = ClientApi::createShared(requestExecutor, objectMapper);

  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
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
  // file
  auto runningSavePathFmt = (boost::format(RUNNING_SAVE_PATH) % CLIENT_NAME).str();
  auto waitingSavePathFmt = (boost::format(WAITING_SAVE_PATH) % CLIENT_NAME).str();
  auto secretSavePathFmt = (boost::format(SECRET_SAVE_PATH) % CLIENT_NAME).str();

  while (true) {
    // 判断是否要更新秘钥
    // 调预生成秘钥接口
    auto genSecretResponse = client->genSecret(shannonnet::CLIENT_NODE);
    auto genSecretMsg = genSecretResponse->readBodyToDto<oatpp::Object<shannonnet::MessageGenSecretDto>>(objectMapper);
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
          continue;
        }
        break;
      }
      LOG(WARNING) << "Api client secret not generate over, serverCreateTime: " << serverCreateTime
                   << ", clientCreateTime: " << clientCreateTime;
      sleep(10);
    }

    // 循环拉秘钥
    std::vector<S_Type> secretA = lwe->generateSecretA(runningSavePath.ToString());
    // 格式化进度消息
    auto logMsgFmt =
      (boost::format("serverNodeId => %u, clientNodeId => %u, index => %s") % serverNodeId % CLIENT_NODE % index).str();
    // 存储接收到的秘钥文件
    Path waitingSavePath(waitingSavePathFmt);
    waitingSavePath = waitingSavePath / index / index + ".bin";
    Path secretSavePath(secretSavePathFmt);
    auto nodeFmt = (boost::format("node_%u_%u") % serverNodeId % CLIENT_NODE).str();
    secretSavePath = secretSavePath / nodeFmt / index + ".bin";
    uint32_t progress = 0;
    uint32_t fileSize = 0;
    while (true) {
      progress = redis.zscore(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET, index).value();
      LOG(INFO) << "Api client receive secret running, logMsg: " << logMsgFmt << ", progress: " << progress;
      auto response = client->getData(CLIENT_NODE, index, progress);
      auto getDataMsg = response->readBodyToDto<oatpp::Object<shannonnet::MessageSecretDto>>(objectMapper);
      if (getDataMsg->statusCode != 200) {
        LOG(ERROR) << "Api client receive secret error, statusCode: " << getDataMsg->statusCode
                   << ", description: " << getDataMsg->description.getValue("");
        break;
      }

      auto secret = getDataMsg->data;
      std::ofstream ofs(waitingSavePath.ToString(), std::ios::out | std::ios::binary | std::ios::app);
      LOG(INFO) << "Api client receive secret processing, logMsg: " << logMsgFmt << ", progress: " << progress
                << ", size: " << secret->size();
      for (auto &item : *secret) {
        auto secretS = item->secretS.getValue("");
        auto secretB = item->secretB.getValue("");
        LOG_ASSERT(!secretS.empty());
        LOG_ASSERT(!secretB.empty());
        // 解密
        auto secretData = lwe->decrypt(secretA, secretS, secretB);
        LOG_ASSERT(!secretData.empty());

        ofs.write(secretData.data(), secretData.size());
      }
      // 判断是否接收完成
      ofs.flush();
      ofs.seekp(0, std::ios::end);
      fileSize = ofs.tellp();
      ofs.close();
      LOG(INFO) << "Api client receive secret write over, logMsg: " << logMsgFmt << ", fileSize: " << fileSize;

      // 接收完成时
      uint32_t secretASize = SN_DEBUG ? DEBUG_SECRET_A_SIZE : SECRET_A_SIZE;
      if (fileSize >= secretASize) {
        LOG(INFO) << "Api client receive secret over, logMsg: " << logMsgFmt << ", fileSize: " << fileSize;
        // 进行crc32校验
        std::vector<char> bufferVec(secretASize);
        std::ifstream ifs(waitingSavePath.ToString(), std::ios::in | std::ios::binary);
        ifs.read(bufferVec.data(), bufferVec.size());
        ifs.close();
        // 发送来的crc32值
        std::string crc32Send{bufferVec.data() + bufferVec.size() - CRC_LEN, CRC_LEN};
        // 根据接收到的秘钥计算crc32值
        uint32_t crc32Cal = crc32c::Crc32c(bufferVec.data(), bufferVec.size() - CRC_LEN);
        std::stringstream crc32SS;
        crc32SS << std::setw(CRC_LEN) << std::setfill('0') << std::to_string(crc32Cal);
        if (crc32SS.str() != crc32Send) {
          LOG(ERROR) << "Api client secret crc32 failed, logMsg: " << logMsgFmt << ", crc32Send: " << crc32Send
                     << ", crc32Cal: " << crc32SS.str();
          client->resultReport(CLIENT_NODE, index, shannonnet::NODE_SECRET_INVALID);
        } else {
          LOG(INFO) << "Api client secret crc32 success, logMsg: " << logMsgFmt << ", crc32Send: " << crc32Send
                    << ", crc32Cal: " << crc32SS.str();
          client->resultReport(CLIENT_NODE, index, shannonnet::NODE_SECRET_VALID);
          std::ofstream secretOfs(secretSavePath.ToString(), std::ios::out | std::ios::binary | std::ios::ate);
          secretOfs.write(bufferVec.data(), bufferVec.size());
          secretOfs.flush();
          secretOfs.close();
          LOG(INFO) << "Api client move secret success, logMsg: " << logMsgFmt
                    << ", secretSavePath: " << secretSavePath.ToString();
        }
        LOG_IF(ERROR, !waitingSavePath.Remove()) << "Api client secret remove failed, logMsg: " << logMsgFmt
                                                 << ", waitingSavePath: " << waitingSavePath.ToString();
        crc32SS.str("");
        break;
      }
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