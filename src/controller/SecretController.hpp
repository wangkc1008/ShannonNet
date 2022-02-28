#ifndef __SHANNON_NET_SERVER_CONTROLLER_HPP__
#define __SHANNON_NET_SERVER_CONTROLLER_HPP__
#include <future>
#include <thread>

#include "boost/format.hpp"
#include "crc32c/crc32c.h"
#include "nlohmann/json.hpp"
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include "src/main/Encrypt.h"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiController)

class SecretController : public oatpp::web::server::api::ApiController {
 public:
  SecretController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
      : oatpp::web::server::api::ApiController(objectMapper) {}

 private:
  uint32_t secretASize_ = SN_DEBUG ? DEBUG_SECRET_A_SIZE : SECRET_A_SIZE;

  /**
   * @brief
   *    传输秘钥前client需要调用改接口生成传输过程所需秘钥
   *
   */
  ENDPOINT("POST", "/gen_secret/", genSecret, BODY_DTO(Object<GenSecretPostDto>, body)) {
    LOG(INFO) << "Api genSecret is running, nodeIdArg: " << body->nodeIdArg;

    uint16_t nodeId = body->nodeIdArg.getValue(0);
    LOG_ASSERT(nodeId > 0);

    auto msg = MessageGenSecretDto::createShared();
    auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

    std::string randomStr;
    auto keyIndexTimeFmt = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % SERVER_NAME).str();
    // 生成随机index
    while (true) {
      randomStr = shannonnet::genRandomStr();
      auto isExist = redis.get(randomStr);
      auto score = redis.zscore(keyIndexTimeFmt, randomStr);
      LOG(INFO) << "Api genSecret, randomStr: " << randomStr << ", isExist: " << *isExist << ", score: " << *score;
      if (!isExist && !score) {
        break;
      }
    }

    // 对已生成的index加锁 当index对应的秘钥全部生成时删除该key 避免重复生成
    auto resSet = redis.set(randomStr, "1");
    if (!resSet) {
      msg->statusCode = 500;
      msg->description = "randomStr set error";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api genSecret " << msg->description.getValue("") << ", randomStr: " << randomStr
                 << ", msg: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 生成随机数
    uint32_t randomNum = shannonnet::genRandomNum();

    // json消息
    nlohmann::json jsonData = {{"source", SERVER_NAME},  {"randomNum", randomNum},
                               {"index", randomStr},     {"serverNodeId", shannonnet::SERVER_NODE},
                               {"clientNodeId", nodeId}, {"value", ""}};
    LOG(INFO) << "Api genSecret json data, jsonData: " << jsonData.dump();
    // 队列通知生成index对应的秘钥文件
    auto resLPush = redis.lpush(shannonnet::KEY_GENERATE_SECRET_LIST, jsonData.dump());
    if (!resLPush) {
      msg->statusCode = 500;
      msg->description = "json lpush error";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api genSecret " << msg->description.getValue("") << ", randomStr: " << randomStr
                 << ", msg: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    msg->statusCode = 200;
    msg->description = "success";
    msg->data = {};

    auto genSecretDto = GenSecretDto::createShared();
    genSecretDto->nodeId = SERVER_NODE;
    genSecretDto->randomNum = randomNum;
    genSecretDto->index = randomStr;
    msg->data = std::move(genSecretDto);
    auto jsonMapperData = jsonObjectMapper->writeToString(msg);
    LOG(INFO) << "Api genSecret success, msg: " << jsonMapperData->c_str();
    return createDtoResponse(Status::CODE_200, msg);
  }

  /**
   * @brief
   *    传输秘钥接口
   *
   */
  ENDPOINT("POST", "/get_data/", getData, BODY_DTO(Object<GetDataPostDto>, body)) {
    // 验证nodeId的合法性 todo
    LOG(INFO) << "Api getData is running, nodeId: " << body->nodeIdArg << ", index: " << body->indexArg.getValue("")
              << ", progress: " << body->progressArg;
    auto startTime = std::chrono::system_clock::now();
    uint16_t nodeId = body->nodeIdArg.getValue(0);
    LOG_ASSERT(nodeId > 0);
    std::string index = body->indexArg.getValue("");
    LOG_ASSERT(!index.empty());
    uint32_t progress = body->progressArg.getValue(0);
    LOG_ASSERT(progress >= 0);

    std::string argsMsg =
      (boost::format("nodeId => %d; index => %s; progress => %d;") % nodeId % index % progress).str();

    auto msg = MessageSecretDto::createShared();
    auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

    // 检查index所需秘钥是否生成完毕
    auto keyIndexTimeFmt = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % SERVER_NAME).str();
    auto createTime = redis.zscore(keyIndexTimeFmt, index);
    if (!createTime) {
      msg->statusCode = 500;
      msg->description = "index error or not generated";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 发送所需的running秘钥是否存在
    auto runningSavePathFmt = (boost::format(RUNNING_SAVE_PATH) % SERVER_NAME).str();
    Path runningSavePath(runningSavePathFmt);
    runningSavePath = runningSavePath / index / index + ".bin";
    if (!runningSavePath.Exists()) {
      msg->statusCode = 500;
      msg->description = "running secret not exist";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", runningSavePath: " << runningSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 待发送的waiting秘钥是否存在
    auto waitingSavePathFmt = (boost::format(WAITING_SAVE_PATH) % SERVER_NAME).str();
    Path waitingSavePath(waitingSavePathFmt);
    waitingSavePath = waitingSavePath / index / index + "_waited.bin";
    if (!waitingSavePath.Exists()) {
      msg->statusCode = 500;
      msg->description = "waiting secret not exist";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", waitingSavePath: " << waitingSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 检查secret 密钥是否已存在
    auto nodeFmt = (boost::format("node_%d_%d") % shannonnet::SERVER_NODE % nodeId).str();
    auto secretSavePathFmt = (boost::format(SECRET_SAVE_PATH) % SERVER_NAME).str();
    Path secretSavePath(secretSavePathFmt);
    secretSavePath = secretSavePath / nodeFmt / (index + ".bin");
    if (secretSavePath.Exists()) {
      msg->statusCode = 500;
      msg->description = "current secret exists";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", secretSavePath: " << secretSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }
  
    // 加密  累计 EACH_NUM 次
    shannonnet::Encrypt::ptr encrypt(new shannonnet::Encrypt(jsonObjectMapper, index, argsMsg,
                                                             runningSavePath.ToString(), waitingSavePath.ToString(),
                                                             secretSavePath.ToString(), nodeId));
  
    bool flag = encrypt->process(progress);
    auto encryptMsg = encrypt->getMsg();
    if (!flag) {
      return createDtoResponse(Status::CODE_500, encryptMsg);
    }

    LOG(INFO) << "Api getData success, args: " << argsMsg << " statusCode " << encryptMsg->statusCode << " description "
              << encryptMsg->description.getValue("") << " size " << encryptMsg->data->size() << ".";
    auto endTime = std::chrono::system_clock::now();
    LOG(WARNING) << "Time: " << (endTime - startTime).count();
    return createDtoResponse(Status::CODE_200, encryptMsg);
  }

  /**
   * @brief
   *    秘钥传输完毕的结果上报接口
   *
   */
  ENDPOINT("POST", "/result_report/", resultReport, BODY_DTO(Object<ResultReportPostDto>, body)) {
    LOG(INFO) << "Api resultReport is running, nodeId: " << body->nodeIdArg
              << ", index: " << body->indexArg.getValue("") << ", isValid: " << body->isValidArg;
    uint16_t nodeId = body->nodeIdArg.getValue(0);
    LOG_ASSERT(nodeId > 0);
    std::string index = body->indexArg.getValue("");
    LOG_ASSERT(!index.empty());
    uint16_t isValid = body->isValidArg.getValue(static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_INVALID));

    std::string argsMsg = (boost::format("nodeId => %d; index => %s; isValid => %d;") % nodeId % index % isValid).str();

    auto msg = MessageDto::createShared();
    auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

    // 验证当前index
    auto keyIndexTimeFmt = (boost::format(shannonnet::KEY_SECRET_INDEX_TIME_ZSET) % SERVER_NAME).str();
    auto createTime = redis.zscore(keyIndexTimeFmt, index);
    if (!createTime) {
      msg->statusCode = 500;
      msg->description = "index error or not generated";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 设置秘钥为valid
    auto redisKeyFmt = (boost::format(shannonnet::KEY_NODE_INDEX_VALID_ZSET) % shannonnet::SERVER_NODE % nodeId).str();
    if (isValid == static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_VALID)) {
      redis.zadd(redisKeyFmt, index, static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_VALID));
      msg->statusCode = 200;
      msg->description = "valid success";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(INFO) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
                << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_200, msg);
    }

    // 设置秘钥invalid 同时删除秘钥文件
    redis.zadd(redisKeyFmt, index, static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_INVALID));

    auto secretSavePathFmt = (boost::format(SECRET_SAVE_PATH) % SERVER_NAME).str();
    auto nodeFmt = (boost::format("node_%d_%d") % shannonnet::SERVER_NODE % nodeId).str();

    Path secretSavePath(secretSavePathFmt);
    secretSavePath = secretSavePath / nodeFmt / (index + ".bin");
    if (!secretSavePath.Exists()) {
      msg->statusCode = 500;
      msg->description = "current secret not exist";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", secretSavePath: " << secretSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    if (!secretSavePath.Remove()) {
      msg->statusCode = 500;
      msg->description = "current secret remove failed";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", secretSavePath: " << secretSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    msg->statusCode = 200;
    msg->description = "success";
    msg->data = {};
    auto jsonMapperData = jsonObjectMapper->writeToString(msg);
    LOG(INFO) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
              << ", json: " << jsonMapperData->c_str();
    return createDtoResponse(Status::CODE_200, msg);
  }
};

#include OATPP_CODEGEN_END(ApiController)
}  // namespace shannonnet
#endif  // __SHANNON_NET_SERVER_CONTROLLER_HPP__