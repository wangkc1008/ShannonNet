#ifndef __SHANNONET_NET_DTO_HPP__
#define __SHANNONET_NET_DTO_HPP__

#include <oatpp/core/data/mapping/type/Object.hpp>
#include <oatpp/core/macro/codegen.hpp>

namespace shannonnet {
#include OATPP_CODEGEN_BEGIN(DTO)

class GenSecretDto : public oatpp::DTO {
  DTO_INIT(GenSecretDto, DTO);
  DTO_FIELD(UInt8, nodeId);
  DTO_FIELD(UInt32, randomNum);
  DTO_FIELD(String, index);
};

class MessageGenSecretDto : public oatpp::DTO {
  DTO_INIT(MessageGenSecretDto, DTO);
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, description);
  DTO_FIELD(Object<GenSecretDto>, data);
};

class SecretDto : public oatpp::DTO {
  DTO_INIT(SecretDto, DTO);
  DTO_FIELD(String, secretS);
  DTO_FIELD(String, secretB);
  DTO_FIELD(UInt32, progress);
};

class MessageSecretDto : public oatpp::DTO {
  DTO_INIT(MessageSecretDto, DTO);
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, description);
  DTO_FIELD(Vector<Object<SecretDto>>, data);
};

class MessageDto : public oatpp::DTO {
  DTO_INIT(MessageDto, DTO);
  DTO_FIELD(Int32, statusCode);
  DTO_FIELD(String, description);
  DTO_FIELD(Vector<String>, data);
};

#include OATPP_CODEGEN_END(DTO)
}  // namespace shannonnet
#endif  // __SHANNONET_NET_DTO_HPP__