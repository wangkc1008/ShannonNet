#include "src/main/Encrypt.h"

namespace shannonnet {
std::vector<SecretDto::Wrapper> encryptSecrets(const shannonnet::LWE<S_Type>::ptr &lwePtr, const char *data,
                                               uint32_t progress, uint32_t offset, const uint32_t runningThreadId,
                                               const std::vector<S_Type> &secretA) {
  uint32_t currentNum =
    (runningThreadId != RUNNING_THREAD_NUM - 1) ? (offset + EACH_NUM / RUNNING_THREAD_NUM) : EACH_NUM;
  std::vector<SecretDto::Wrapper> vecSecretDto;
  vecSecretDto.reserve(currentNum - offset);

  for (size_t i = offset; i < currentNum; ++i) {
    auto vec = lwePtr->encrypt({data + i * S_LEN, S_LEN}, secretA);
    auto secret = SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = progress++;
    vecSecretDto.emplace_back(std::move(secret));
  }
  return vecSecretDto;
}

std::vector<SecretDto::Wrapper> encryptSecretsGPU(const shannonnet::LWE<S_Type>::ptr &lwePtr, const char *data,
                                                  uint32_t index, uint32_t offset, const uint32_t runningThreadId,
                                                  const torch::Tensor &secretA, uint32_t progress) {
  uint32_t currentNum =
    (runningThreadId != RUNNING_THREAD_NUM - 1) ? (offset + EACH_NUM / RUNNING_THREAD_NUM) : EACH_NUM;
  std::vector<SecretDto::Wrapper> vecSecretDto;
  vecSecretDto.reserve(currentNum - offset);
  TIMERSTART(encrypt)
  for (size_t i = offset; i < currentNum; ++i) {
    auto vec = lwePtr->encryptGPU({data + i * S_LEN, S_LEN}, secretA, progress);
    auto secret = SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = index++;
    vecSecretDto.emplace_back(std::move(secret));
  }
  TIMERSTOP(encrypt)
  return vecSecretDto;
}

Encrypt::Encrypt(const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper, const std::string &index,
                 const std::string &logFmt, const std::string &runningSavePath, const std::string &waitingSavePath,
                 const std::string &secretSavePath, const uint16_t nodeId, std::uint32_t progress)
    : ShannonNet(jsonObjectMapper, index, logFmt, runningSavePath, waitingSavePath, secretSavePath), nodeId_(nodeId) {
  msg_->statusCode = 200;
  msg_->description = "success";
  msg_->data = {};

  secretA_ = torch::from_file(runningSavePath_, false, M * N, torch::TensorOptions().dtype(SAVE_GPU_TYPE))
          .reshape({M, N})
          .to(torch::Device{torch::kCUDA, static_cast<c10::DeviceIndex>(progress % GPU_NUM)}, GPU_TYPE);
}

Encrypt::~Encrypt(){};

bool Encrypt::process(const uint32_t progress) {
  std::ifstream ifs(waitingSavePath_, std::ios::in | std::ios::binary);
  if (!ifs.is_open()) {
    msg_->statusCode = 500;
    msg_->description = "waiting secret open failed";
    msg_->data = {};
    auto jsonMapperData = jsonObjectMapper_->writeToString(msg_);
    LOG(ERROR) << "Api getData " << msg_->description.getValue("") << ", args: " << logFmt_
               << ", waitingSavePath: " << waitingSavePath_ << ", json: " << jsonMapperData->c_str();
    return false;
  }

  // 判断当前请求的进度是否等于redis中的进度记录 或 是否大于秘钥文件大小
  uint32_t redisProgress = 0;
  auto serverIndexCount = (boost::format(shannonnet::KEY_SECRET_INDEX_COUNT_ZSET) % SERVER_NAME).str();
  if (progress >= PROGRESS) {
    redisProgress = redis_.zscore(serverIndexCount, index_).value();
    auto secret = SecretDto::createShared();
    secret->secretB = "";
    secret->secretS = "";
    secret->progress = redisProgress - 1;

    msg_->statusCode = 201;
    msg_->description = "secret progress error";
    msg_->data->emplace_back(std::move(secret));
    auto jsonMapperData = jsonObjectMapper_->writeToString(msg_);
    LOG(ERROR) << "Api getData " << msg_->description.getValue("") << ", args: " << logFmt_
               << ", redisProgress: " << redisProgress << ", json: " << jsonMapperData->c_str();
    return false;
  }

  // 读取指定进度位置的秘钥内容
  ifs.seekg(progress * S_LEN * EACH_NUM, std::ios::beg);
  std::vector<char> bufferVec(S_LEN * EACH_NUM);
  ifs.read(bufferVec.data(), S_LEN * EACH_NUM);
  ifs.close();
  std::vector<std::future<std::vector<SecretDto::Wrapper>>> vec;
  for (size_t runningThreadId = 0; runningThreadId < RUNNING_THREAD_NUM; ++runningThreadId) {
    uint32_t offset = EACH_NUM / RUNNING_THREAD_NUM * runningThreadId;
    vec.emplace_back(std::async(std::launch::async, shannonnet::encryptSecretsGPU, lwePtr_, bufferVec.data(),
                                progress * EACH_NUM + offset, offset, runningThreadId, secretA_, progress));
  }
  for (auto &futureItem : vec) {
    auto res = futureItem.get();  // 堵塞 直到就绪
    for (auto &item : res) {
      msg_->data->emplace_back(std::move(item));
    }
  }
  // 根据redis中的进度记录判断当前秘钥文件是否传输完毕
  redisProgress = redis_.zincrby(serverIndexCount, 1, index_);
  LOG_IF(ERROR, !redisProgress) << "Api getData incrby error, args: " << logFmt_;
  if (redisProgress >= PROGRESS) {
    return move();
  }
  return true;
}

/**
 * @brief 将原秘钥从waiting move到 secrets
 *
 * @return true
 * @return false
 */
bool Encrypt::move() {
  auto redisKeyFmt = (boost::format(shannonnet::KEY_NODE_INDEX_VALID_ZSET) % shannonnet::SERVER_NODE % nodeId_).str();
  // 当前秘钥是否已添加到节点下的通信秘钥zset中
  uint16_t isValid = redis_.zscore(redisKeyFmt, index_).value();
  if (isValid) {
    msg_->statusCode = 500;
    msg_->description = "current secret have been added";
    msg_->data = {};
    auto jsonMapperData = jsonObjectMapper_->writeToString(msg_);
    LOG(ERROR) << "Api getData " << msg_->description.getValue("") << ", args: " << logFmt_
               << ", redisKeyFmt: " << redisKeyFmt << ", isValid: " << isValid << " json: " << jsonMapperData->c_str();
    return false;
  }

  {
    std::vector<char> moveBufferVec(secretASize_);
    std::ifstream ifs(waitingSavePath_, std::ios::in | std::ios::binary);
    ifs.read(moveBufferVec.data(), moveBufferVec.size());

    std::ofstream ofs(secretSavePath_, std::ios::out | std::ios::binary | std::ios::ate);
    ofs.write(moveBufferVec.data(), moveBufferVec.size());
    ofs.flush();
    ofs.close();
  }

  // 设置当前秘钥为invalid状态
  LOG_IF(ERROR, !redis_.zadd(redisKeyFmt, index_, static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_INVALID)))
    << "Api getData zadd error, args: " << logFmt_ << ", key: " << redisKeyFmt;
  // 删除waiting秘钥
  Path waitingSavePath(waitingSavePath_);
  LOG_IF(ERROR, !waitingSavePath.Remove())
    << "Api getData remove failed, args: " << logFmt_ << ", path: " << waitingSavePath_;

  LOG(INFO) << "Api getData move secret success, args: " << logFmt_ << ", beforePath: " << waitingSavePath_
            << ", afterPath: " << secretSavePath_;
  return true;
}
}  // namespace shannonnet