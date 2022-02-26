#include "src/Decrypt.h"

namespace shannonnet {
std::vector<std::string> decryptSecrets(const shannonnet::LWE<S_Type>::ptr &lwePtr,
                                        const std::vector<shannonnet::SecretDto::Wrapper>::iterator &begin,
                                        const uint32_t runningThreadId, const std::vector<S_Type> &secretA) {
  uint64_t currentNum = (runningThreadId != RUNNING_THREAD_NUM - 1)
                          ? EACH_NUM / RUNNING_THREAD_NUM
                          : (EACH_NUM - runningThreadId * EACH_NUM / RUNNING_THREAD_NUM);
  std::vector<std::string> secretVec;
  secretVec.reserve(currentNum);
  auto end = begin + currentNum;
  for (auto iter = begin; iter != end; ++iter) {
    auto secretS = (*iter)->secretS.getValue("");
    auto secretB = (*iter)->secretB.getValue("");
    LOG_ASSERT(!secretS.empty());
    LOG_ASSERT(!secretB.empty());
    // 解密
    auto secretData = lwePtr->decrypt(secretA, secretS, secretB);
    LOG_ASSERT(!secretData.empty());
    secretVec.emplace_back(std::move(secretData));
  }
  return secretVec;
}

Decrypt::Decrypt(const std::shared_ptr<shannonnet::ClientApi> &client,
                 const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper, const std::string &index,
                 const std::string &logFmt, const std::string &runningSavePath, const std::string &waitingSavePath,
                 const std::string &secretSavePath)
    : client_(client),
      jsonObjectMapper_(jsonObjectMapper),
      index_(index),
      logFmt_(logFmt),
      runningSavePath_(runningSavePath),
      waitingSavePath_(waitingSavePath),
      secretSavePath_(secretSavePath) {
  secretA_ = lwePtr_->generateSecretA(runningSavePath_);
}

Decrypt::~Decrypt() {}

bool Decrypt::process(const uint32_t progress) {
  std::string logMsgFmt = logFmt_ + (boost::format(", progress => %d") % progress).str();
  LOG(INFO) << "Api client running, logMsg: " << logMsgFmt;
  auto getDataPost = GetDataPostDto::createShared();
  getDataPost->indexArg = index_;
  getDataPost->progressArg = progress;
  auto response = client_->getData(getDataPost);
  auto getDataMsg = response->readBodyToDto<oatpp::Object<shannonnet::MessageSecretDto>>(jsonObjectMapper_);
  if (getDataMsg->statusCode != 200) {
    LOG(ERROR) << "Api client error, statusCode: " << getDataMsg->statusCode
               << ", description: " << getDataMsg->description.getValue("");
    return false;
  }
  auto startTime = std::chrono::system_clock::now();
  auto secret = getDataMsg->data;

  LOG(INFO) << "Api client processing, logMsg: " << logMsgFmt << ", size: " << secret->size();
  std::vector<std::future<std::vector<std::string>>> vec;
  for (size_t runningThreadId = 0; runningThreadId < RUNNING_THREAD_NUM; ++runningThreadId) {
    uint32_t offset = EACH_NUM / RUNNING_THREAD_NUM * runningThreadId;
    vec.push_back(std::async(std::launch::async, shannonnet::decryptSecrets, lwePtr_, secret->begin() + offset,
                             runningThreadId, secretA_));
  }

  Path waitingSavePath(waitingSavePath_);
  waitingSavePath = waitingSavePath / (index_ + ".bin." + std::to_string(progress));
  std::ofstream ofs(waitingSavePath.ToString(), std::ios::out | std::ios::binary | std::ios::app);
  for (auto &futureItem : vec) {
    auto res = futureItem.get();  // 堵塞 直到返回值就绪
    for (auto &secrets : res) {
      ofs.write(secrets.data(), secrets.size());
    }
  }

  ofs.flush();
  ofs.seekp(0, std::ios::end);
  uint32_t fileSize = ofs.tellp();
  ofs.close();
  LOG(INFO) << "Api client write over, logMsg: " << logMsgFmt << ", currentFileSize: " << fileSize;
  LOG(WARNING) << "Time: " << (std::chrono::system_clock::now() - startTime).count();
  return true;
}

bool Decrypt::gather(const uint32_t progress) {
  auto startTime = std::chrono::system_clock::now();
  std::string logMsgFmt = logFmt_ + (boost::format(", progress => %d") % progress).str();
  LOG(INFO) << "Api client gathering, logMsg: " << logMsgFmt;
  // 进行crc32校验
  std::vector<char> bufferVec(secretASize_);
  Path waitingSavePath(waitingSavePath_);
  for (size_t num = 0; num < progress; ++num) {
    auto waitingSaveFile = waitingSavePath / (index_ + ".bin." + std::to_string(num));
    if (!waitingSaveFile.Exists()) {
      LOG(ERROR) << "Api Client file not exist, file: " << waitingSaveFile.ToString();
      continue;
    }
    std::ifstream ifs(waitingSaveFile.ToString(), std::ios::in | std::ios::binary);
    ifs.read(bufferVec.data() + num * EACH_NUM * S_LEN, EACH_NUM * S_LEN);
    ifs.close();
    // LOG_IF(ERROR, !waitingSaveFile.Remove()) << "Api client delete file failed, file: " << waitingSaveFile.ToString();
  }
  // 发送来的crc32值
  std::string crc32Send{bufferVec.data() + bufferVec.size() - CRC_LEN, CRC_LEN};
  // 根据接收到的秘钥计算crc32值
  uint32_t crc32Cal = crc32c::Crc32c(bufferVec.data(), bufferVec.size() - CRC_LEN);
  std::stringstream crc32SS;
  crc32SS << std::setw(CRC_LEN) << std::setfill('0') << std::to_string(crc32Cal);
  auto resultReportPost = ResultReportPostDto::createShared();
  resultReportPost->indexArg = index_;
  if (crc32SS.str() != crc32Send) {
    resultReportPost->isValidArg = static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_INVALID);
    LOG(ERROR) << "Api client crc32 failed, logMsg: " << logMsgFmt << ", crc32Send: " << crc32Send
               << ", crc32Cal: " << crc32SS.str();
    client_->resultReport(resultReportPost);
  } else {
    resultReportPost->isValidArg = static_cast<uint16_t>(SECRET_STATUS::NODE_SECRET_VALID);
    LOG(INFO) << "Api client crc32 success, logMsg: " << logMsgFmt << ", crc32Send: " << crc32Send
              << ", crc32Cal: " << crc32SS.str();
    client_->resultReport(resultReportPost);
    std::ofstream ofs(secretSavePath_, std::ios::out | std::ios::binary | std::ios::ate);
    ofs.write(bufferVec.data(), bufferVec.size());
    ofs.flush();
    ofs.close();
    LOG(INFO) << "Api client move secret success, logMsg: " << logMsgFmt << ", beforePath:" << waitingSavePath_
              << ", afterPath: " << secretSavePath_;
  }

  // LOG_IF(ERROR, !waitingSavePath.Remove())
  //   << "Api client dir remove failed, logMsg: " << logMsgFmt << ", waitingSavePath: " << waitingSavePath_;
  crc32SS.str("");
  LOG(WARNING) << "Time: " << (std::chrono::system_clock::now() - startTime).count();
  return true;
}
}  // namespace shannonnet