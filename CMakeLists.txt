cmake_minimum_required(VERSION 3.0.0)
project(ShannonNet)

# 显示构建过程中更详细的错误信息
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -pthread -Wall -Werror -Wno-unused-function -Wno-unused-local-typedefs -Wno-unused-variable -Wno-write-strings -Wno-unused-result")

# 去哪个路径下找头文件 默认去/usr/include找头文件 相当于编译时的-I
include_directories(.)  # 表示当前目录
message(${PROJECT_SOURCE_DIR})
# link_directories() 去哪个路径下找链接的库文件 相当于编译时的-L

set(LIB_SRC
    src/utils/thread.cc
    src/utils/path.cc
    src/utils/utils.cc)

# 生成动态库文件ShannonNet.so
add_library(ShannonNet SHARED ${LIB_SRC})

# 必须找到Eigen3包
find_package(Eigen3 REQUIRED)
# 找到后默认设置_INCLUDE_DIRS环境变量
include_directories(${EIGEN3_INCLUDE_DIRS})
# /usr/include/eigen3
message(${EIGEN3_INCLUDE_DIRS})

# 找到pthread的动态库文件 /usr/lib/x86_64-linux-gnu/libpthread.so
find_library(PTHREAD pthread)
message(${PTHREAD})
# 找到crc32的动态库文件 /usr/local/lib/libcrc32c.so
find_library(CRC32C crc32c)
message(${CRC32C})
# 找到oatpp的静态库文件 /usr/local/lib/oatpp-1.3.0/liboatpp.a
find_library(OATPP oatpp)
message(${OATPP})

find_library(GLOG 0.6.0 glog)

# 构建子路径下的CMakeLists.txt
# add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/glog)
# message(${GLOG_INCLUDE_DIRS})

# 设置变量LIBS 包括前面的所有库文件
set(LIBS
    ShannonNet
    pthread
    crc32c
    glog
    oatpp)

# 由tests/lwe_test.cc生成可执行文件lwe_test
add_executable(lwe_test tests/lwe_test.cc)
# 提醒编译器lwe_test依赖于ShannonNet，需要先生成ShannonNet.so再去生成lwe_test
add_dependencies(lwe_test ShannonNet)
# lwe_test生成需要链接LIBS中的动态/静态库文件相当于编译时的-l
target_link_libraries(lwe_test ${LIBS})

# add_executable(server_test tests/server_test.cc)
# add_dependencies(server_test ShannonNet)
# target_link_libraries(server_test ${LIBS})

# add_executable(client_test tests/client_test.cc)
# add_dependencies(client_test ShannonNet)
# target_link_libraries(client_test ${LIBS})

# add_executable(oatpp_test tests/oatpp_test.cc)
# add_dependencies(oatpp_test ShannonNet)
# target_link_libraries(oatpp_test ${LIBS})

add_executable(oatpp_server src/ServerApp.cc)
target_link_libraries(oatpp_server ${LIBS})

add_executable(oatpp_client src/ClientApp.cc)
target_link_libraries(oatpp_client ${LIBS})

# 可执行文件保存路径
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 库文件保存路径
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
