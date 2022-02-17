#include <iostream>
#include <unistd.h>
#include <vector>
#include <string>

#include "sw/redis++/redis++.h"
#include "nlohmann/json.hpp"

#include "src/config/config.h"
#include "src/utils/utils.h"
#include "src/utils/redis.h"

void test01() {
    auto connectionOptions = shannonnet::initRedisConnectionOptions();
    auto redis = sw::redis::Redis(connectionOptions);
    std::vector<std::string> vec{"123456", "0", "haha"};
    nlohmann::json j = {
        {"index", "461cdscnc515asik"},
        {"serverNodeId", 0},
        {"clientNodeId", 1},
        {"value", "haha"}
    };
    std::cout << j.dump() << std::endl;
    redis.lpush(shannonnet::KEY_GENERATE_SECRET_LIST, j.dump());
}

int main(int argc, char **argv) {
  test01();
  return 0;
}