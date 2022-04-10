#ifndef __SHANNON_NET_BASE_H__
#define __SHANNON_NET_BASE_H__

#include <future>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>

#include "boost/format.hpp"

#include "src/ClientComponent.hpp"
#include "src/LWE.hpp"
#include "src/app/ClientApi.hpp"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
class ShannonNet {
 protected:
  const uint32_t secretASize_ = SN_DEBUG ? DEBUG_SECRET_A_SIZE : SECRET_A_SIZE;

 protected:
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr_{new shannonnet::LWE<shannonnet::S_Type>()};
  std::shared_ptr<oatpp::data::mapping::ObjectMapper> jsonObjectMapper_;
  std::string index_;
  std::string logFmt_;
  std::string runningSavePath_;
  std::string waitingSavePath_;
  std::string secretSavePath_;
  std::vector<S_Type> secretA_;

 public:
  typedef std::shared_ptr<ShannonNet> ptr;

  ShannonNet(const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper, const std::string &index,
             const std::string &logFmt, const std::string &runningSavePath, const std::string &waitingSavePath,
             const std::string &secretSavePath)
      : jsonObjectMapper_(jsonObjectMapper),
        index_(index),
        logFmt_(logFmt),
        runningSavePath_(runningSavePath),
        waitingSavePath_(waitingSavePath),
        secretSavePath_(secretSavePath) {
    secretA_ = lwePtr_->generateSecretA(runningSavePath_);
  };

  virtual ~ShannonNet() {};                            // 虚析构
  bool virtual process(const uint32_t progress) = 0;  // 纯虚函数 子类必须重写
};
}  // namespace shannonnet

#endif