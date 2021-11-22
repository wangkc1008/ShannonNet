#ifndef __SHANNON_NET_CLIENT_API_HPP__
#define __SHANNON_NET_CLIENT_API_HPP__

#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/web/client/ApiClient.hpp>

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiClient)
class ClientApi : public oatpp::web::client::ApiClient {
  API_CLIENT_INIT(ClientApi);
  API_CALL("GET", "/get_data/{nodeId}", getData, PATH(UInt16, nodeId));
  API_CALL("GET", "/question_report/{nodeId}/{fileFlag}", questionReport, PATH(UInt16, nodeId), PATH(String, fileFlag));
};
#include OATPP_CODEGEN_END(ApiClient)
}  // namespace shannonnet
#endif  // __SHANNON_NET_CLIENT_API_HPP__
