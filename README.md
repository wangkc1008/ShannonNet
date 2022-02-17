# ShannonNet
## 预先安装 
[crc32](https://github.com/google/crc32c) <br>
[oatpp](https://github.com/oatpp/oatpp) <br>
[glog](https://github.com/google/glog#getting-started) <br>
Eigen3 <br>
## 编译
mkdir build && cd build <br>
cmake .. <br>
make <br>
## 运行
目前只完成加解密、http传输密钥和密钥文件保存 <br>
bin/oatpp_server 为服务端可执行文件 <br>
bin/oatpp_client 为客户端可执行文件 <br>

服务端接口文件 <br>
src/controller/SecretController.hpp 目前只提供
1. get_data/{nodeId} 通过客户端的nodeId获取密钥
2. question_report/{nodeId}/{fileFlag} 如果客户端验证获取到的密钥不正确时会通知服务端删除已保存的密钥文件（待改进）

客户端文件 <br>
src/ClientApp.cc 拉取服务端接口并且保存解密后的密钥文件 <br>

配置 <br>
src/config/config.h 包括密钥配置，服务端、客户端和日志等的基础配置 <br>
