#ifndef __SHANNON_NET_CONFIG_H__
#define __SHANNON_NET_CONFIG_H__

#include <iostream>
#include <string>

#define SN_DEBUG 0

namespace shannonnet {
// debug
constexpr uint32_t DEBUG_SECRET_A_SIZE = 64 * 1024 * 1024;  // debug时单个密钥A大小

typedef uint16_t S_Type;
// waiting secret
constexpr uint32_t SECRET_A_SIZE = 1024 * 1024 * 1024;  // 单个密钥A的大小为 => 1073741824 Bytes
constexpr uint16_t SAVE_INDEX_LEN = 16;                 // 密钥的全局唯一index长度

// running secret
constexpr uint S_LEN = 1024;         // 每次传输的密文Bytes => 1k
constexpr uint CRC_LEN = 10;         // 2^32 最长为10位
constexpr uint BIT = 8;              // 间隔多少bit处理一次
constexpr uint32_t M = S_LEN * BIT;  // 1024 * 8 = 8192
constexpr uint32_t N = M;
constexpr uint16_t Q = 97;
constexpr uint16_t EACH_NUM = 1024;                            // 每次发送1024个S_LEN长的秘钥
constexpr uint32_t SECRET_FILE_SIZE = M * N * sizeof(S_Type);  // running secret大小
// 2 => 5478ms 4 => 4511ms  6 => 4689ms  8 => 5187ms
constexpr uint16_t RUNNING_THREAD_NUM = 4;  // 运行时加解密线程数量
constexpr uint16_t CLIENT_THREAD_NUM = 4;   // 同时有多少client在获取密钥
constexpr uint32_t PROGRESS =
  SN_DEBUG ? (DEBUG_SECRET_A_SIZE / EACH_NUM / S_LEN) : (SECRET_A_SIZE / EACH_NUM / S_LEN);  // 进度

// generate secret
constexpr uint16_t THREAD_NUM = 2;   // 为每个有效密钥文件开启两个线程
constexpr uint16_t FILE_NUM = 8;     // 生成密钥时有效密钥文件的最大使用数量
constexpr uint16_t READ_LEN = 128;   // 每次取128个bit 异或得到1bit
constexpr uint16_t READ_NUM = 1024;  // 取1024次 相当于可得到128bit密钥
constexpr char *SECRET_SAVE_PATH = "/home/wangkc/shannonnet/%s/secrets/";  // server/client正在使用的秘钥存储路径
constexpr char *RUNNING_SAVE_PATH = "/home/wangkc/shannonnet/%s/running/";  // server/client运行时需要的秘钥存储路径
constexpr char *WAITING_SAVE_PATH = "/home/wangkc/shannonnet/%s/waiting/";  // server/client待发送的秘钥存储路径
constexpr char *LOG_GEN_SECRET = "generate_secret";

// server
constexpr char *SERVER_NAME = "server";
constexpr char *SERVER_ADDRESS = "127.0.0.1";
constexpr uint16_t SERVER_PORT = 8000;
constexpr uint16_t SERVER_NODE = 0;
constexpr char *LOG_NAME_SERVER = "shannonnet_server";

// client
constexpr char *CLIENT_NAME = "client";
constexpr char *CLIENT_ADDRESS = "10.102.32.192";
constexpr uint16_t CLIENT_PORT = 80;
constexpr uint16_t CLIENT_NODE = 1;
constexpr char *LOG_NAME_CLIENT = "shannonnet_client";

// log
constexpr char *LOG_PATH = "/tmp/shannonnet/logs";

// redis
constexpr char *REDIS_HOST = "127.0.0.1";
constexpr uint16_t REDIS_PORT = 6379;
constexpr char *REDIS_PASSWORD = "admin";
constexpr uint32_t REDIS_SOCKET_TIME = 2000;
constexpr char *KEY_GENERATE_SECRET_LIST = "shannonnet_gen_secret_list";  // 生成秘钥的消息队列
constexpr char *KEY_SECRET_INDEX_TIME_ZSET =
  "shannonnet_%s_save_index_time_zset";  // shannonnet_{SERVER_NAME/CLIENT_NAME}_save_index_time_zset 保存生成完毕的秘钥
                                         // index => create_time
constexpr char *KEY_SECRET_INDEX_COUNT_ZSET =
  "shannonnet_%s_count_index_num_zset";  // shannonnet_{SERVER_NAME/CLIENT_NAME}_count_index_num_zset
                                         // 秘钥和读取次数 index => num
constexpr char *KEY_NODE_INDEX_VALID_ZSET =
  "shannonnet_node_%d_%d_index_valid_zset";  // shannonnet_node_{SERVER_NODE}_{CLIENT_NODE}_index_valid_zset
                                             // 每个节点下的所有有效的秘钥文件 index => valid: 1 valid; 2 invalid
enum class SECRET_STATUS : std::uint16_t { NODE_SECRET_VALID = 1, NODE_SECRET_INVALID = 2 };
// init
constexpr char *LOG_NAME_INIT = "shannonnet_init";

}  // namespace shannonnet

#endif  // __SHANNON_NET_CONFIG_H__