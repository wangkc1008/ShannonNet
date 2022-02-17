#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <fstream>

#include "nlohmann/json.hpp"

#include "src/utils/path.h"
#include "src/utils/log.h"
#include "src/utils/redis.h"

void test01() {
  auto connectionOptions = shannonnet::initRedisConnectionOptions();
  auto redis = sw::redis::Redis(connectionOptions);
  auto val = redis.get("www");
  if (val) {
    std::cout << *val << std::endl;
  } else {
    std::cout << "not exist" << std::endl;
  }
  while (true) {
    try {
      auto val2 = redis.rpop("queue");
      if (val2) {
        std::cout << typeid(*val2).name() << std::endl;
        std::cout << *val2 << std::endl;
        auto jsonVal = nlohmann::json::parse(*val2);
        if (jsonVal.find("index2") == jsonVal.end()) {
          std::stringstream ss;
          for (auto &item : jsonVal.items()) {
            ss << "\n\t" << item.key() << "->" << item.value();
          }
          std::cout << "no index2, but got " + ss.str() << std::endl;
        }
        std::cout << jsonVal["index2"] << std::endl;
      } else {
        std::cout << "not exist" << std::endl;
      }
      sleep(2);
    } catch (const sw::redis::Error &e) {
      std::cout << e.what() << std::endl;
    }
  }
}

void test02() {
  shannonnet::Path path("/home/wangkc/shannonnet/secrets/node_0_1/");
  if (!path.IsDirectory()) {
    std::cout << "Not dir" << std::endl;
    return;
  }
  auto dIt = shannonnet::Path::DirIterator::OpenDirectory(&path);
  if (dIt == nullptr) {
    std::cout << "Open failed" << std::endl;
    return;
  }

  while (dIt->HasNext()) {
    try {
      shannonnet::Path secretFilePath = dIt->Next();
      if (secretFilePath.Extension() != ".bin") {
        continue;
      }
      std::ifstream ifs(secretFilePath.ToString(), std::ios::in | std::ios::binary);
      ifs.seekg(0, std::ios::end);
      auto size = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      std::vector<char> vec(size);
      std::cout << "size " << vec.size() << std::endl;
      ifs.read(vec.data(), vec.size());
      for (uint16_t i = 0; i < 10; ++i) {
        std::cout << secretFilePath.ToString() << i * 1000 << ":" << vec[i * 1000] << std::endl;
      }
      for (uint16_t j = 0; j < 10; ++j) {
        std::cout << secretFilePath.ToString() << 536870912 + j * 1000 << ":" << vec[536870912 + j * 1000] << std::endl;
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
    }
  }
}

int main(int argc, char **argv) {
  shannonnet::InitLog(shannonnet::LOG_NAME_CLIENT);
  test02();
  return 0;
}