#ifndef __SHANNON_NET_CLIENT_API_HPP__
#define __SHANNON_NET_CLIENT_API_HPP__

#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/web/client/ApiClient.hpp>

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiClient)
class ClientApi : public oatpp::web::client::ApiClient {
  API_CLIENT_INIT(ClientApi);
  API_CALL("GET", "/shannonnet/gen_secret/{nodeId}/", genSecret, PATH(UInt16, nodeId));
  API_CALL("GET", "/shannonnet/get_data/{nodeIdArg}/{indexArg}/{progressArg}/", getData, PATH(UInt16, nodeIdArg),
           PATH(String, indexArg), PATH(UInt32, progressArg));
  API_CALL("GET", "/shannonnet/result_report/{nodeIdArg}/{indexArg}/{isValidArg}/", resultReport, PATH(UInt16, nodeIdArg),
           PATH(String, indexArg), PATH(UInt8, isValidArg));
};
#include OATPP_CODEGEN_END(ApiClient)
}  // namespace shannonnet
#endif  // __SHANNON_NET_CLIENT_API_HPP__
