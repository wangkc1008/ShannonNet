#ifndef __SHANNON_NET_LOG_H__
#define __SHANNON_NET_LOG_H__

#include "glog/logging.h"

#include "src/config/config.h"

namespace shannonnet {
static void InitLog(const char *cmd) {
  google::InitGoogleLogging(cmd);
  FLAGS_log_dir = LOG_PATH;                // 日志路径
  FLAGS_log_prefix = "";                   // 日志前缀
  FLAGS_alsologtostderr = true;            // 写入文件的同时是否std输出
  FLAGS_logbufsecs = 0;                    // 缓冲日志的最大秒数 0表示实时输出
  FLAGS_minloglevel = google::GLOG_INFO;   // 最小处理日志的级别 只输出该级别以上的日志
  FLAGS_max_log_size = 1024;               // 最大日志文件大小（MB） 超出时会对日志文件分割
  FLAGS_stop_logging_if_full_disk = true;  // 磁盘已满时停止输出
  FLAGS_colorlogtostderr = true;           // std输出的日志colorful
}
}  // namespace shannonnet
#endif  // __SHANNON_NET_LOG_H__