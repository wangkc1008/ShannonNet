#ifndef __SHANNON_NET_H__
#define __SHANNON_NET_H__

#include <future>
#include <iostream>
#include <string>
#include <vector>

#include "boost/format.hpp"

#include "src/ClientComponent.hpp"
#include "src/LWE.hpp"
#include "src/app/ClientApi.hpp"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
class Decrypt {
 private:
  static const uint32_t secretASize_ = SN_DEBUG ? DEBUG_SECRET_A_SIZE : SECRET_A_SIZE;

 private:
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr_{new shannonnet::LWE<shannonnet::S_Type>()};
  std::shared_ptr<shannonnet::ClientApi> client_;
  std::shared_ptr<oatpp::data::mapping::ObjectMapper> jsonObjectMapper_;
  std::string index_;
  std::string logFmt_;
  std::string runningSavePath_;
  std::string waitingSavePath_;
  std::string secretSavePath_;
  std::vector<S_Type> secretA_;

 public:
  typedef std::shared_ptr<Decrypt> ptr;
  Decrypt(const std::shared_ptr<shannonnet::ClientApi> &client,
          const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper, const std::string &index,
          const std::string &logFmt, const std::string &runningSavePath, const std::string &waitingSavePath,
          const std::string &secretSavePath);
  ~Decrypt();

  bool process(const uint32_t progress);

  bool gather(const uint32_t progress);
};
}  // namespace shannonnet
#endif