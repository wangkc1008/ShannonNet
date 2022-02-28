cmake_minimum_required(VERSION 3.0.0)
project(ShannonNet)

# 显示构建过程中更详细的错误信息
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++14 -pthread -Wall -Werror -Wno-unused-function -Wno-unused-local-typedefs -Wno-unused-variable -Wno-write-strings -Wno-unused-result -Wno-sign-compare")

# 引入conan
set(CONAN_DISABLE_CHECK_COMPILER ON)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# 可执行文件保存路径
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/)
# 库文件保存路径
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/)

# 添加.cc文件
set(LIB_SRC
    src/utils/path.cc
    src/utils/utils.cc
)

include_directories(${CMAKE_SOURCE_DIR}/)
add_library(${PROJECT_NAME} STATIC ${LIB_SRC})
# 生成libShannonNet.a需要链接其他库文件
target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})

# 由tests/lwe_test.cc生成可执行文件lwe_test
add_executable(lwe_test tests/lwe_test.cc)
# 提醒编译器lwe_test依赖于ShannonNet，需要先生成ShannonNet.so再去生成lwe_test
add_dependencies(lwe_test ${PROJECT_NAME})
# target_include_directories(lwe_test PRIVATE ${CMAKE_SOURCE_DIR}/)
# lwe_test生成需要链接LIBS中的动态/静态库文件相当于编译时的-l
target_link_libraries(lwe_test ${PROJECT_NAME})

add_executable(redis_test tests/redis_test.cc)
add_dependencies(redis_test ${PROJECT_NAME})
target_link_libraries(redis_test ${PROJECT_NAME})

add_executable(json_test tests/json_test.cc)
add_dependencies(json_test ${PROJECT_NAME})
target_link_libraries(json_test ${PROJECT_NAME})

add_executable(generator src/scripts/generateSecretFile.cc)
add_dependencies(generator ${PROJECT_NAME})
target_link_libraries(generator ${PROJECT_NAME})

add_executable(oatpp_server src/ServerApp.cc src/main/Encrypt.cc)
add_dependencies(oatpp_server ${PROJECT_NAME})
target_link_libraries(oatpp_server ${PROJECT_NAME})

add_executable(oatpp_client src/ClientApp.cc src/main/Decrypt.cc)
add_dependencies(oatpp_client ${PROJECT_NAME})
target_link_libraries(oatpp_client ${PROJECT_NAME})

add_executable(shannonnet_init init/init.cc)
add_dependencies(shannonnet_init ${PROJECT_NAME})
target_link_libraries(shannonnet_init ${PROJECT_NAME})