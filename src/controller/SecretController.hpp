#ifndef __SHANNON_NET_SERVER_CONTROLLER_HPP__
#define __SHANNON_NET_SERVER_CONTROLLER_HPP__

#include "crc32c/crc32c.h"
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include "src/LWE.hpp"
#include "src/common.h"
#include "src/dto/DTOs.hpp"

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(ApiController)
class SecretController : public oatpp::web::server::api::ApiController {
 public:
  SecretController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
      : oatpp::web::server::api::ApiController(objectMapper) {}

  ENDPOINT("GET", "/get_data/{nodeId}", getData, PATH(UInt16, nodeId)) {
    // 验证nodeId的合法性 todo
    LOG(INFO) << "getData nodeId " << nodeId << " start.";

    auto msg = MessageSecretDto::createShared();

    typedef uint T;
    shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());
    auto secretHttp = lwe->generate_secret_http(S_LEN / sizeof(T), 1);

    uint32_t flag = crc32c::Crc32c(secretHttp);
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << std::to_string(flag);
    secretHttp += ss.str();

    std::cout << ss.str() << std::endl;

    Path serverPath(SERVER_SECRET_PATH);
    serverPath = serverPath / ("server_" + std::to_string(SERVER_NODE) + "_" + std::to_string(nodeId));
    if (!serverPath.Exists()) {
      serverPath.CreateDirectory();
    }
    std::string curTime = shannonnet::getCurTime(true);
    serverPath = serverPath / ("secret_" + curTime + ".bin");
    std::ofstream ofs(serverPath.ToString(), std::ios::out | std::ios::binary | std::ios::ate);
    if (!ofs.is_open()) {
      LOG(ERROR) << "getData file open failed: " << serverPath.ToString() << ".";
      msg->statusCode = 500;
      msg->description = "secret save failed.";
      msg->data = {};
      return createDtoResponse(Status::CODE_500, msg);
    }
    ofs << secretHttp;
    ofs.flush();
    ofs.close();
    LOG(INFO) << "getData clientNodeId " << nodeId << " savePath " << serverPath << " server_save_secret_success.";

    auto vec = lwe->encrypt(secretHttp);
    auto secret = SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->fileFlag = curTime;

    msg->statusCode = 200;
    msg->description = "success";
    msg->data = {};
    msg->data->push_back(secret);
    // auto jsonObjectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    // oatpp::String json = jsonObjectMapper->writeToString(msg);
    // LOG(INFO) << json->c_str();
    LOG(INFO) << "getData nodeId " << nodeId << " statusCode " << msg->statusCode << " description "
              << msg->description.getValue("") << " size " << msg->data->size() << ".";
    return createDtoResponse(Status::CODE_200, msg);
  }

  ENDPOINT("GET", "question_report/{nodeId}/{fileFlag}", questionReport, PATH(UInt16, nodeId), PATH(String, fileFlag)) {
    std::cout << nodeId << std::endl;
    std::cout << fileFlag->c_str() << std::endl;
    
    auto msg = MessageDto::createShared();
    msg->statusCode = 200;
    msg->description = "success";
    msg->data = {};
    auto status = Status::CODE_200;

    Path serverPath(SERVER_SECRET_PATH);
    serverPath = serverPath / ("server_" + std::to_string(SERVER_NODE) + "_" + std::to_string(nodeId)) / ("secret_" + std::string(fileFlag) + ".bin");
    if (!serverPath.Exists()) {
      msg->statusCode = 404;
      msg->description = "file not found";
      msg->data = {};
      status = Status::CODE_404;
    }

    if (!serverPath.Remove()) {
      msg->statusCode = 500;
      msg->description = "file remove failed";
      msg->data = {};
      status = Status::CODE_500;
    }

    LOG(INFO) << "questionReport nodeId " << nodeId << " fileFlag " << fileFlag->c_str() << " statusCode " << msg->statusCode << " description "
              << msg->description.getValue("") << ".";
    return createDtoResponse(status, msg);
  }
};
#include OATPP_CODEGEN_END(ApiController)
}  // namespace shannonnet
#endif  // __SHANNON_NET_SERVER_CONTROLLER_HPP__