#ifndef __SHANNON_NET_CONFIG_H__
#define __SHANNON_NET_CONFIG_H__

#include <iostream>
#include <string>

namespace shannonnet {
// secret
constexpr uint S_LEN = 1024;  // 每次传输的密文Bytes
constexpr uint CRC_LEN = 10;  // 2^32 最长为10位
constexpr uint BIT = 8;
constexpr uint64_t M = (S_LEN + CRC_LEN) * BIT;  // (1024 + 10) * 8 = 8272
constexpr uint64_t N = M;
constexpr uint16_t Q = 97;

// server
constexpr char *SERVER_ADDRESS = "127.0.0.1";
constexpr uint16_t SERVER_PORT = 8000;
constexpr uint16_t SERVER_NODE = 0;
constexpr char *SERVER_SECRET_PATH = "/mnt/HDD/wangkc/shannonnet";
constexpr char *LOG_NAME_SERVER = "shannonnet_server";

// client
constexpr char *CLIENT_ADDRESS = "10.102.32.65";
constexpr uint16_t CLIENT_PORT = 80;
constexpr uint16_t CLIENT_NODE = 1;
constexpr char *CLIENT_SECRET_PATH = "/mnt/HDD/wangkc/shannonnet";
constexpr char *LOG_NAME_CLIENT = "shannonnet_client";

// log
constexpr char *LOG_PATH = "/tmp/shannonnet/logs";

// temporary
constexpr char *SECRET_A_PATH = "/mnt/HDD/wangkc/shannonnet/secret.bin";
}  // namespace shannonnet

#endif  // __SHANNON_NET_CONFIG_H__