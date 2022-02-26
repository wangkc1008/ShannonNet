#ifndef __SHANNON_NET_CLIENT_COMPONENT_HPP__
#define __SHANNON_NET_CLIENT_COMPONENT_HPP__

#include <memory>

#include <oatpp/core/macro/component.hpp>
#include <oatpp/network/tcp/client/ConnectionProvider.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>

#include "config/config.h"

namespace shannonnet {
class ClientComponent {
 public:
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ClientConnectionProvider>, clientConnectionProvider)
  ([] {
    return oatpp::network::tcp::client::ConnectionProvider::createShared(
      {CLIENT_ADDRESS, CLIENT_PORT, oatpp::network::Address::IP_4});
  }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper)
  ([] { return oatpp::parser::json::mapping::ObjectMapper::createShared(); }());
};
}  // namespace shannonnet

#endif  // __SHANNON_NET_CLIENT_COMPONENT_HPP__