#include <oatpp/network/Server.hpp>

#include "ServerComponent.hpp"
#include "common.h"
#include "controller/SecretController.hpp"
#include "dto/DTOs.hpp"

namespace shannonnet {
void serverRun() {
  shannonnet::ServerComponent component;
  OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
  router->addController(std::make_shared<SecretController>());

  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler);
  OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connectionProvider);
  oatpp::network::Server server(connectionProvider, connectionHandler);

  LOG(INFO) << "Server running on " << connectionProvider->getProperty("port").toString().getValue("") << ".";
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