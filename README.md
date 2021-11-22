# ShannonNet
## 预先安装 
[crc32](https://github.com/google/crc32c)
[oatpp](https://github.com/oatpp/oatpp)
[glog](https://github.com/google/glog#getting-started)
Eigen3
## 编译
mkdir build && cd build
cmake ..
make
## 运行
目前只完成加解密、http传输密钥和密钥文件保存
bin/oatpp_server 为服务端可执行文件
bin/oatpp_client 为客户端可执行文件

服务端接口文件
src/controller/SecretController.hpp 目前只提供
1. get_data/{nodeId} 通过客户端的nodeId获取密钥
2. question_report/{nodeId}/{fileFlag} 如果客户端验证获取到的密钥不正确时会通知服务端删除已保存的密钥文件（待改进）

客户端文件
src/ClientApp.cc拉取服务端接口并且保存解密后的密钥文件

配置
src/config/config.h 包括密钥配置，服务端、客户端和日志等的基础配置
