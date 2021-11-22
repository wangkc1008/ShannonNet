#include <memory>

#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>

#include OATPP_CODEGEN_BEGIN(DTO)

class MessageDto : public oatpp::DTO {
  DTO_INIT(MessageDto, DTO);
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, message);
};

#include OATPP_CODEGEN_END(DTO)

class Handler : public oatpp::web::server::HttpRequestHandler {
 private:
  std::shared_ptr<oatpp::data::mapping::ObjectMapper> m_objectMapper;

 public:
  Handler(const std::shared_ptr<oatpp::data::mapping::ObjectMapper> &objectMapper) : m_objectMapper(objectMapper) {}

  std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest> &request) override {
    auto message = MessageDto::createShared();
    message->statusCode = 1024;
    message->message = "Hello DTO";
    return ResponseFactory::createResponse(Status::CODE_200, message, m_objectMapper);
  }
};

void run() {
  auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
  auto router = oatpp::web::server::HttpRouter::createShared();
  router->route("GET", "/hello", std::make_shared<Handler>(objectMapper));
  auto connectionHandler = oatpp::web::server::HttpConnectionHandler::createShared(router);
  auto connectionProvider =
    oatpp::network::tcp::server::ConnectionProvider::createShared({"127.0.0.1", 8000, oatpp::network::Address::IP_4});
  oatpp::network::Server server(connectionProvider, connectionHandler);
  OATPP_LOGI("MyApp", "Server running on port %s", connectionProvider->getProperty("port").getData());
  server.run();
}

int main(int argc, char **argv) {
  oatpp::base::Environment::init();
  run();
  oatpp::base::Environment::destroy();
  return 0;
}