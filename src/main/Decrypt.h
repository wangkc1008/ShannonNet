#ifndef __SHANNON_NET_DECRYPT_H__
#define __SHANNON_NET_DECRYPT_H__

#include "src/main/ShannonNet.h"

namespace shannonnet {
class Decrypt : public ShannonNet {
 private:
  std::shared_ptr<shannonnet::ClientApi> client_;

 public:
  typedef std::shared_ptr<Decrypt> ptr;
  Decrypt(const std::shared_ptr<shannonnet::ClientApi> &client,
          const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &jsonObjectMapper, const std::string &index,
          const std::string &logFmt, const std::string &runningSavePath, const std::string &waitingSavePath,
          const std::string &secretSavePath);
  ~Decrypt();

  bool process(const uint32_t progress) override;

  bool gather(const uint32_t progress);
};
}  // namespace shannonnet
#endif