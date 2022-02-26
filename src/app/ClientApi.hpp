#ifndef __SHANNON_NET_CLIENT_API_HPP__
#define __SHANNON_NET_CLIENT_API_HPP__

#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/web/client/ApiClient.hpp>

#include "src/dto/DTOs.hpp"

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiClient)
class ClientApi : public oatpp::web::client::ApiClient {
  API_CLIENT_INIT(ClientApi);
  API_CALL("POST", "/shannonnet/gen_secret/", genSecret, BODY_DTO(Object<GenSecretPostDto>, body));
  API_CALL("POST", "/shannonnet/get_data/", getData,
           BODY_DTO(Object<GetDataPostDto>, body));
  API_CALL("POST", "/shannonnet/result_report/", resultReport,
           BODY_DTO(Object<ResultReportPostDto>, body));
};
#include OATPP_CODEGEN_END(ApiClient)
}  // namespace shannonnet
#endif  // __SHANNON_NET_CLIENT_API_HPP__
