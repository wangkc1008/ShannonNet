#ifndef __SHANNON_NET_REDIS_H__
#define __SHANNON_NET_REDIS_H__

#include "sw/redis++/redis++.h"

#include "src/config/config.h"

namespace shannonnet {
static sw::redis::ConnectionOptions initRedisConnectionOptions() {
  sw::redis::ConnectionOptions connectionOptions;
  connectionOptions.host = REDIS_HOST;
  connectionOptions.port = REDIS_PORT;
  connectionOptions.password = REDIS_PASSWORD;
  // By default, the timeout is 0ms, i.e. never timeout and block until we send or receive successful.
  connectionOptions.socket_timeout = std::chrono::milliseconds(REDIS_SOCKET_TIME);
  return connectionOptions;
}
}  // namespace shannonnet
#endif
