#include "time.h"
#include "crc32c/crc32c.h"
#include "glog/logging.h"
#include "src/LWE.hpp"

#include <oatpp/algorithm/CRC.hpp>

void test() {
  typedef uint T;
  shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());
  std::vector<std::string> vec = lwe->encrypt("Hell", {});
  auto s = vec[0];
  auto b = vec[1];
  std::cout << sizeof(s) << std::endl;
  std::cout << s << std::endl;
  std::cout << sizeof(b) << std::endl;
  std::cout << b << std::endl;
  std::cout << "--" << crc32c::Crc32c(s) << "--" << std::endl;
  std::cout << "--" << crc32c::Crc32c(b) << "--" << std::endl;
  std::string m;
  m = lwe->decrypt({}, s, b);
  std::cout << m << std::endl;
}

void test02() {
  std::vector<uint> a = {1, 2, 3};
  std::string s((char *)a.data(), sizeof(uint) * a.size());
  std::cout << s << std::endl;
  std::vector<uint> b(s.data(), s.data() + s.size());
  std::cout << b.size() << std::endl;
}

void test03() {
  std::string s{"abcd"};
  // auto res1 = crc32c::Extend()
  auto res1 = crc32c::Crc32c(s);
  std::cout << res1 << std::endl;
  std::cout << s << std::endl;
  auto res2 = crc32c::Extend(5, reinterpret_cast<const uint8_t *>(s.data()), s.size());
  std::cout << res2 << std::endl;
  std::cout << s << std::endl;
}

void test04() {
  typedef uint T;
  using MatrixXT = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
  std::ifstream ifs("send.txt", std::ios::in | std::ios::binary);
  std::vector<T> vec;
  vec.reserve(shannonnet::S_LEN / sizeof(T));
  ifs.read((char *)vec.data(), shannonnet::S_LEN);
  std::cout << vec.size() << std::endl;
  Eigen::Map<MatrixXT> map((T *)vec.data(), 1024 / 4, 1);
  std::cout << map << std::endl;
}

int main(int argc, char **argv) {
  // InitLog(shannonnet::LOG_NAME);
  // LOG(INFO) << "Hello World";
  // LOG(WARNING) << "Hello World";
  // test03();
  std::cout << sizeof(long uint) << std::endl;
  typedef uint16_t T;
  std::cout << sizeof(T) << std::endl;
  time_t startTimestamp = time(nullptr);
  shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());
  // lwe->generate_secret_file();
  time_t endTimestamp = time(nullptr);
  std::cout << endTimestamp - startTimestamp << std::endl;
  // test05();
  return 0;
}