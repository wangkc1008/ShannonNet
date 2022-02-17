#ifndef __SHANNON_NET_SERVER_CONTROLLER_HPP__
#define __SHANNON_NET_SERVER_CONTROLLER_HPP__

#include "boost/format.hpp"
#include "crc32c/crc32c.h"
#include "nlohmann/json.hpp"
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include "src/LWE.hpp"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiController)
class SecretController : public oatpp::web::server::api::ApiController {
 public:
  SecretController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
      : oatpp::web::server::api::ApiController(objectMapper) {}
  /**
   * @brief
   *    传输秘钥前client需要调用改接口生成传输过程所需秘钥
   *
   */
  ENDPOINT("GET", "/gen_secret/{nodeIdArg}/", genSecret, PATH(UInt16, nodeIdArg)) {
    LOG(INFO) << "Api genSecret is running, nodeIdArg: " << nodeIdArg;

    uint16_t nodeId = nodeIdArg.getValue(0);
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
  ENDPOINT("GET", "/get_data/{nodeIdArg}/{indexArg}/{progressArg}", getData, PATH(UInt16, nodeIdArg),
           PATH(String, indexArg), PATH(UInt32, progressArg)) {
    // 验证nodeId的合法性 todo
    LOG(INFO) << "Api getData is running, nodeId: " << nodeIdArg << ", index: " << indexArg.getValue("")
              << ", progress: " << progressArg;
    uint16_t nodeId = nodeIdArg.getValue(0);
    LOG_ASSERT(nodeId > 0);
    std::string index = indexArg.getValue("");
    LOG_ASSERT(!index.empty());
    uint32_t progress = progressArg.getValue(0);
    LOG_ASSERT(progress >= 0);

    std::string argsMsg =
      (boost::format("nodeId => %d; index => %s; progress => %d;") % nodeId % index % progress).str();

    auto msg = MessageSecretDto::createShared();
    auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto redis = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

    msg->statusCode = 200;
    msg->description = "success";
    msg->data = {};

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

    // 读取waiting秘钥
    std::ifstream ifs(waitingSavePath.ToString(), std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
      msg->statusCode = 500;
      msg->description = "waiting secret open failed";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", waitingSavePath: " << waitingSavePath.ToString() << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_500, msg);
    }

    // 判断当前请求的进度是否等于redis中的进度记录 或 是否大于秘钥文件大小
    ifs.seekg(0, std::ios::end);
    uint32_t fileSize = ifs.tellg();
    uint32_t redisProgress = redis.zscore(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET, index).value();
    if (progress != redisProgress || progress >= fileSize) {
      auto secret = SecretDto::createShared();
      secret->secretB = "";
      secret->secretS = "";
      secret->progress = redisProgress - 1;

      msg->statusCode = 201;
      msg->description = "secret progress error";
      msg->data->emplace_back(std::move(secret));
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                 << ", redisProgress: " << redisProgress << ", fileSize: " << fileSize
                 << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_201, msg);
    }
    // 读取指定进度位置的秘钥内容
    ifs.seekg(progress * S_LEN, std::ios::beg);
    std::vector<char> bufferVec(S_LEN * EACH_NUM);
    ifs.read(bufferVec.data(), S_LEN * EACH_NUM);

    // 加密  累计 EACH_NUM 次
    shannonnet::LWE<S_Type>::ptr lwe(new shannonnet::LWE<S_Type>(runningSavePath.ToString()));
    for (uint16_t i = 0; i < EACH_NUM; ++i) {
      auto vec = lwe->encrypt({bufferVec.data() + i * S_LEN, S_LEN});
      auto secret = SecretDto::createShared();
      secret->secretS = vec[0];
      secret->secretB = vec[1];
      secret->progress = progress++;
      msg->data->emplace_back(std::move(secret));
      LOG_IF(ERROR, !redis.zincrby(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET, 1, index))
        << "Api getData incrby error, args: " << argsMsg;
    }

    // 根据redis中的进度记录判断当前秘钥文件是否传输完毕
    redisProgress = redis.zscore(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET, index).value();
    if (redisProgress >= (SECRET_A_SIZE / S_LEN)) {
      auto redisKeyFmt =
        (boost::format(shannonnet::KEY_NODE_INDEX_VALID_ZSET) % shannonnet::SERVER_NODE % nodeId).str();
      // 当前秘钥是否已添加到节点下的通信秘钥zset中
      uint16_t isValid = redis.zscore(redisKeyFmt, index).value();
      if (isValid) {
        msg->statusCode = 500;
        msg->description = "current secret have been added";
        msg->data = {};
        auto jsonMapperData = jsonObjectMapper->writeToString(msg);
        LOG(ERROR) << "Api getData " << msg->description.getValue("") << ", args: " << argsMsg
                   << ", redisKeyFmt: " << redisKeyFmt << ", isValid: " << isValid
                   << " json: " << jsonMapperData->c_str();
        return createDtoResponse(Status::CODE_500, msg);
      }

      // 将原秘钥从waiting move到 secrets
      ifs.seekg(0, std::ios::beg);
      std::vector<char> moveBufferVec(SECRET_A_SIZE);
      ifs.read(moveBufferVec.data(), moveBufferVec.size());

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

      std::ofstream ofs(secretSavePath.ToString(), std::ios::out | std::ios::binary | std::ios::ate);
      ofs.write(moveBufferVec.data(), moveBufferVec.size());
      ofs.flush();
      ofs.close();
      // 设置当前秘钥为invalid状态
      LOG_IF(ERROR, !redis.zadd(redisKeyFmt, index, shannonnet::NODE_SECRET_INVALID))
        << "Api getData zadd error, args: " << argsMsg << ", key: " << redisKeyFmt;
      // 删除waiting秘钥
      LOG_IF(ERROR, !waitingSavePath.Remove())
        << "Api getData remove failed, args: " << argsMsg << ", path: " << waitingSavePath.ToString();

      LOG(INFO) << "Api getData move secret success, args: " << argsMsg
                << ", beforePath: " << waitingSavePath.ToString() << ", afterPath: " << secretSavePath.ToString();
    }

    ifs.close();

    LOG(INFO) << "Api getData success, args: " << argsMsg << " statusCode " << msg->statusCode << " description "
              << msg->description.getValue("") << " size " << msg->data->size() << ".";
    return createDtoResponse(Status::CODE_200, msg);
  }

  /**
   * @brief
   *    秘钥传输完毕的结果上报接口
   *
   */
  ENDPOINT("GET", "/result_report/{nodeIdArg}/{indexArg}/{isValidArg}", resultReport, PATH(UInt16, nodeIdArg),
           PATH(String, indexArg), PATH(UInt8, isValidArg)) {
    LOG(INFO) << "Api resultReport is running, nodeId: " << nodeIdArg << ", index: " << indexArg.getValue("")
              << ", isValid: " << isValidArg;
    uint16_t nodeId = nodeIdArg.getValue(0);
    LOG_ASSERT(nodeId > 0);
    std::string index = indexArg.getValue("");
    LOG_ASSERT(!index.empty());
    uint16_t isValid = isValidArg.getValue(shannonnet::NODE_SECRET_INVALID);

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
    if (isValid == shannonnet::NODE_SECRET_VALID) {
      redis.zadd(redisKeyFmt, index, shannonnet::NODE_SECRET_VALID);
      msg->statusCode = 200;
      msg->description = "valid success";
      msg->data = {};
      auto jsonMapperData = jsonObjectMapper->writeToString(msg);
      LOG(INFO) << "Api resultReport " << msg->description.getValue("") << ", args: " << argsMsg
                << ", json: " << jsonMapperData->c_str();
      return createDtoResponse(Status::CODE_200, msg);
    }

    // 设置秘钥invalid 同时删除秘钥文件
    LOG_IF(ERROR, !redis.zadd(redisKeyFmt, index, shannonnet::NODE_SECRET_INVALID))
      << "Api resultReport set secret invalid error, args: " << argsMsg;

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