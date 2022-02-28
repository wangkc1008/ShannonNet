#ifndef __SHANNON_NET_ENCRYPT_H__
#define __SHANNON_NET_ENCRYPT_H__

#include "src/main/ShannonNet.h"

namespace shannonnet {
class Encrypt : public ShannonNet {
 private:
  uint16_t nodeId_;
  shannonnet::MessageSecretDto::Wrapper msg_ = MessageSecretDto::createShared();
  sw::redis::Redis redis_ = sw::redis::Redis(shannonnet::initRedisConnectionOptions());

 public:
  typedef std::shared_ptr<Encrypt> ptr;
  Encrypt(const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper,
          const std::string &index, const std::string &logFmt, const std::string &runningSavePath,
          const std::string &waitingSavePath, const std::string &secretSavePath, const uint16_t nodeId);
  ~Encrypt();

  bool process(const uint32_t progress) override;
  bool move();

  const shannonnet::MessageSecretDto::Wrapper &getMsg() const { return msg_; }
};
}  // namespace shannonnet

#endif