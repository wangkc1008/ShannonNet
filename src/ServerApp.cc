#include <oatpp/network/Server.hpp>

#include "ServerComponent.hpp"
#include "common.h"
#include "controller/SecretController.hpp"
#include "dto/DTOs.hpp"

namespace shannonnet {
void serverRun() {
  shannonnet::ServerComponent components;
  auto router = components.httpRouter.getObject();
  router->addController(std::make_shared<SecretController>());

  oatpp::network::Server server(components.serverConnectionProvider.getObject(),
                                components.serverConnectionHandler.getObject());
  LOG(INFO) << "Server running on "
            << components.serverConnectionProvider.getObject()->getProperty("port").toString().getValue("") << ".";
  LOG(INFO) << "Server nodeId is " << SERVER_NODE << ".";
  server.run();
}
}  // namespace shannonnet

int main() {
  shannonnet::InitLog(shannonnet::LOG_NAME_SERVER);
  oatpp::base::Environment::init();
  shannonnet::serverRun();
  std::cout << "\nEnvironment:\n";
  std::cout << "objectsCount = " << oatpp::base::Environment::getObjectsCount() << "\n";
  std::cout << "objectsCreated = " << oatpp::base::Environment::getObjectsCreated() << "\n\n";
  oatpp::base::Environment::destroy();
  return 0;
}