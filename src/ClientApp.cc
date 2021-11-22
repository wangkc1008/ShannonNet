#include "crc32c/crc32c.h"
#include <oatpp/web/client/HttpRequestExecutor.hpp>

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
  auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(clientConnectionProvider);
  OATPP_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper);
  auto client = ClientApi::createShared(requestExecutor, objectMapper);

  typedef uint T;
  shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());

  Path clientPath(CLIENT_SECRET_PATH);
  clientPath = clientPath / ("client_" + std::to_string(SERVER_NODE) + "_" + std::to_string(CLIENT_NODE));
  if (!clientPath.Exists()) {
    clientPath.CreateDirectory();
  }

  size_t i = 0;
  while (i++ < 1) {
    auto response = client->getData(CLIENT_NODE);
    auto message = response->readBodyToDto<oatpp::Object<shannonnet::MessageSecretDto>>(objectMapper);

    if (message->statusCode != 200) {
      LOG(INFO) << "statusCode " << message->statusCode << " description " << message->description.getValue("") << ".";
      continue;
    }

    auto secret = message->data;
    LOG(INFO) << "statusCode " << message->statusCode << " description " << message->description.getValue("")
              << " size " << secret->size() << ".";

    std::stringstream ss;
    for (const auto &item : *secret) {
      auto curTime = shannonnet::getCurTime(true);
      auto clientFile = (clientPath / ("secret_" + curTime + ".bin")).ToString();
      std::ofstream ofs(clientFile, std::ios::out | std::ios::binary | std::ios::ate);
      if (!ofs.is_open()) {
        LOG(ERROR) << "file open failed: " << clientFile << ".";
        client->questionReport(CLIENT_NODE, item->fileFlag);
        continue;
      }

      auto secret_s = item->secretS.getValue("");
      auto secret_b = item->secretB.getValue("");
      LOG_ASSERT(!secret_s.empty());
      LOG_ASSERT(!secret_b.empty());

      LOG(INFO) << "secretS " << secret_s.size() << ".";
      LOG(INFO) << "secretB " << secret_b.size() << ".";

      auto s = lwe->decrypt(secret_s, secret_b);
      LOG(INFO) << "decrypt " << s.size() << ".";
      LOG_ASSERT(!s.empty());

      auto res = crc32c::Crc32c(s.substr(0, s.size() - 10));
      ss << std::setw(10) << std::setfill('0') << std::to_string(res);
      if (ss.str() != s.substr(s.size() - 10)) {
        LOG(ERROR) << "clientNodeId " << CLIENT_NODE << " fileFlag " << item->fileFlag.getValue("") << " server "
                   << s.substr(s.size() - 10) << " client " << ss.str();
        client->questionReport(CLIENT_NODE, item->fileFlag);
        continue;
      }
      ss.clear();

      ofs << s;
      ofs.flush();
      ofs.close();
      LOG(INFO) << "serverNodeId " << SERVER_NODE << " savePath " << clientPath << " client save secret success.";
    }
    LOG(INFO) << "current " << i << ".";
  }
  LOG(INFO) << "over.";
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