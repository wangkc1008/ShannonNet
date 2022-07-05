#include "crc32c/crc32c.h"
#include "src/LWE.hpp"
#include "src/config/config.h"
#include "time.h"

#include <thread>
#include <unistd.h>

#include "src/dto/DTOs.hpp"
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

std::vector<shannonnet::SecretDto::Wrapper> encryptSecrets(const shannonnet::LWE<shannonnet::S_Type>::ptr &lwePtr,
                                                           const char *data, uint32_t progress, uint32_t offset,
                                                           const std::vector<shannonnet::S_Type> &secretA,
                                                           const uint32_t runningThreadId) {
  uint32_t currentNum = (runningThreadId != shannonnet::RUNNING_THREAD_NUM - 1)
                          ? (offset + shannonnet::EACH_NUM / shannonnet::RUNNING_THREAD_NUM)
                          : shannonnet::EACH_NUM;
  std::vector<shannonnet::SecretDto::Wrapper> vecSecretDto;
  vecSecretDto.reserve(currentNum - offset);
  TIMERSTART(progress)
  for (size_t i = offset; i < currentNum; ++i) {
    // TIMERSTART(encrypt)
    auto vec = lwePtr->encrypt({data + i * shannonnet::S_LEN, shannonnet::S_LEN}, secretA);
    // TIMERSTOP(encrypt)
    // TIMERSTART(SecretDto)
    auto secret = shannonnet::SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = progress++;
    vecSecretDto.emplace_back(secret);
    // TIMERSTOP(SecretDto)
  }
  TIMERSTOP(progress)
  return vecSecretDto;
}

void test05(uint32_t progress) {
  // auto startTime = std::chrono::system_clock::now();
  TIMERSTART(total)
  TIMERSTART(init)
  std::vector<shannonnet::S_Type> secretA_;
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr_{new shannonnet::LWE<shannonnet::S_Type>()};
  secretA_ = lwePtr_->generateSecretA("/tmp/shannonnet/2685SShX61y1I28U.bin");

  shannonnet::MessageSecretDto::Wrapper msg_ = shannonnet::MessageSecretDto::createShared();
  msg_->statusCode = 200;
  msg_->description = "success";
  msg_->data = {};

  std::ifstream ifs("/tmp/shannonnet/2685SShX61y1I28U_waited.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.read(bufferVec.data(), shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.close();
  TIMERSTOP(init)
  TIMERSTART(multi)
  std::vector<std::future<std::vector<shannonnet::SecretDto::Wrapper>>> vec;
  for (size_t runningThreadId = 0; runningThreadId < shannonnet::RUNNING_THREAD_NUM; ++runningThreadId) {
    uint32_t offset = shannonnet::EACH_NUM / shannonnet::RUNNING_THREAD_NUM * runningThreadId;
    vec.emplace_back(std::async(std::launch::async, encryptSecrets, lwePtr_, bufferVec.data(),
                                progress * shannonnet::EACH_NUM + offset, offset, secretA_, runningThreadId));
  }

  for (auto &futureItem : vec) {
    auto res = futureItem.get();  // 堵塞 直到就绪
    for (auto &item : res) {
      msg_->data->emplace_back(item);
    }
  }
  TIMERSTOP(multi)
  std::cout << msg_->data->size() << std::endl;
  // std::chrono::duration<double> time = std::chrono::system_clock::now() - startTime;
  // std::cout << "time " << time.count() << std::endl;
  TIMERSTOP(total)
}

void test06(uint32_t progress) {
  TIMERSTART(total)
  TIMERSTART(init)
  std::vector<shannonnet::S_Type> secretA_;
  shannonnet::LWE<shannonnet::S_Type>::ptr lwePtr_{new shannonnet::LWE<shannonnet::S_Type>()};
  secretA_ = lwePtr_->generateSecretA("/tmp/shannonnet/2685SShX61y1I28U.bin");

  shannonnet::MessageSecretDto::Wrapper msg_ = shannonnet::MessageSecretDto::createShared();
  msg_->statusCode = 200;
  msg_->description = "success";
  msg_->data = {};

  std::ifstream ifs("/tmp/shannonnet/2685SShX61y1I28U_waited.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.read(bufferVec.data(), shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.close();
  TIMERSTOP(init)
  TIMERSTART(multi)
  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  std::vector<shannonnet::S_Type> secretA = lwe->generateSecretA("/tmp/shannonnet/09w4Y5pubYm7jVSf.bin");
  for (uint16_t i = 0; i < shannonnet::EACH_NUM; ++i) {
    auto vec = lwe->encrypt({bufferVec.data() + i * shannonnet::S_LEN, shannonnet::S_LEN}, secretA);
    auto secret = shannonnet::SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = progress++;
    msg_->data->emplace_back(std::move(secret));
  }
  TIMERSTOP(multi)
  std::cout << msg_->data->size() << std::endl;
  TIMERSTOP(total)
}

void test_encrypt_cpu(uint32_t progress) {
  TIMERSTART(total)
  TIMERSTART(init)
  shannonnet::MessageSecretDto::Wrapper msg_ = shannonnet::MessageSecretDto::createShared();
  msg_->statusCode = 200;
  msg_->description = "success";
  msg_->data = {};

  std::ifstream ifs("/home/wangkc/demo/shannonnet/secret_1.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.read(bufferVec.data(), shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.close();
  TIMERSTOP(init)
  TIMERSTART(multi)
  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  std::vector<shannonnet::S_Type> secretA = lwe->generateSecretA("/home/wangkc/demo/shannonnet/secret_2.bin");
  for (uint16_t i = 0; i < shannonnet::EACH_NUM; ++i) {
    auto vec = lwe->encrypt({bufferVec.data() + i * shannonnet::S_LEN, shannonnet::S_LEN}, secretA);
    auto secret = shannonnet::SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = progress++;
    msg_->data->emplace_back(std::move(secret));
  }
  TIMERSTOP(multi)
  std::cout << msg_->data->size() << std::endl;
  TIMERSTOP(total)
}

void test_cpu() {
  TIMERSTART(total)
  TIMERSTART(init)
  std::ifstream ifs("/home/wangkc/demo/shannonnet/secret_1.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.read(bufferVec.data(), shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.close();
  TIMERSTOP(init)
  TIMERSTART(multi)
  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  std::vector<shannonnet::S_Type> secretA = lwe->generateSecretA("/home/wangkc/demo/shannonnet/secret_2.bin");
  std::string msg{bufferVec.data() + 0 * shannonnet::S_LEN, shannonnet::S_LEN};
  auto vec = lwe->encrypt(msg, secretA);
  auto result = lwe->decrypt(secretA, vec[0], vec[1]);
  assert(msg == result);
  TIMERSTOP(multi)
  TIMERSTOP(total)
}

void test_encrypt_gpu(uint32_t progress) {
  TIMERSTART(total)
  TIMERSTART(init)
  shannonnet::MessageSecretDto::Wrapper msg_ = shannonnet::MessageSecretDto::createShared();
  msg_->statusCode = 200;
  msg_->description = "success";
  msg_->data = {};

  std::ifstream ifs("/home/wangkc/demo/shannonnet/secret_1.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.read(bufferVec.data(), shannonnet::S_LEN * shannonnet::EACH_NUM);
  ifs.close();
  TIMERSTOP(init)
  TIMERSTART(A)
  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  auto gpuId = static_cast<c10::DeviceIndex>(progress % shannonnet::GPU_NUM);
  // std::vector<shannonnet::S_Type> secretA = lwe->generateSecretA("/home/wangkc/demo/shannonnet/secret_2.bin");
  auto secretA = torch::from_file("/home/wangkc/demo/shannonnet/secret_2.bin", false, shannonnet::M * shannonnet::N,
                                  torch::TensorOptions().dtype(torch::kInt16))
                   .reshape({shannonnet::M, shannonnet::N})
                   .to(torch::Device{torch::kCUDA, gpuId}, shannonnet::GPU_TYPE);
  TIMERSTOP(A)
  TIMERSTART(multi)
  for (uint16_t i = 0; i < shannonnet::EACH_NUM; ++i) {
    auto vec = lwe->encryptGPU({bufferVec.data() + i * shannonnet::S_LEN, shannonnet::S_LEN}, secretA, progress);
    auto secret = shannonnet::SecretDto::createShared();
    secret->secretS = vec[0];
    secret->secretB = vec[1];
    secret->progress = progress++;
    msg_->data->emplace_back(std::move(secret));
  }
  TIMERSTOP(multi)
  std::cout << msg_->data->size() << std::endl;
  TIMERSTOP(total)
}

void test_gpu(uint32_t progress) {
  TIMERSTART(total)
  TIMERSTART(init)
  std::ifstream ifs("/home/wangkc/demo/shannonnet/secret_1.bin", std::ios::in | std::ios::binary);
  std::vector<char> bufferVec(shannonnet::S_LEN);
  ifs.read(bufferVec.data(), shannonnet::S_LEN);
  ifs.close();
  shannonnet::LWE<shannonnet::S_Type>::ptr lwe(new shannonnet::LWE<shannonnet::S_Type>());
  TIMERSTART(read)
  auto secretA = torch::from_file("/home/wangkc/demo/shannonnet/secret_2.bin", false, shannonnet::M * shannonnet::N,
                                  torch::TensorOptions().dtype(shannonnet::SAVE_GPU_TYPE))
                   .reshape({shannonnet::M, shannonnet::N});
  TIMERSTOP(read)
  TIMERSTART(to)
  auto gpuId = static_cast<c10::DeviceIndex>(progress % shannonnet::GPU_NUM);
  secretA = secretA.to(torch::Device{torch::kCUDA, gpuId}, shannonnet::GPU_TYPE);
  // torch::save(secretA, "/home/wangkc/demo/ShannonNet/tests/secretA.pt");
  TIMERSTOP(to)
  // TIMERSTART(load)
  // torch::Tensor secretA2;
  // torch::load(secretA2, "/home/wangkc/demo/ShannonNet/tests/secretA.pt");
  // std::cout << secretA2.dtype() << "-" << secretA2.device() << std::endl;
  // TIMERSTOP(load)
  // std::cout << "**************secretA**************" << std::endl;
  // std::cout << secretA << std::endl;
  std::string msg{bufferVec.data(), shannonnet::S_LEN};
  // std::cout << "**************msg**************" << std::endl;
  // std::cout << msg << std::endl;
  TIMERSTOP(init)
  TIMERSTART(multi)
  auto vec = lwe->encryptGPU(msg, secretA, progress);
  // LOG(WARNING) << "s"
  //              << "-" << crc32c::Crc32c(vec[0]);
  // std::ofstream ofs_s("/home/wangkc/demo/ShannonNet/tmp/server_s.bin", std::ios::out | std::ios::binary);
  // ofs_s.write(vec[0].data(), vec[0].size());
  // ofs_s.close();
  // LOG(WARNING) << "b"
  //              << "-" << crc32c::Crc32c(vec[1]);
  // std::ofstream ofs_b("/home/wangkc/demo/ShannonNet/tmp/server_b.bin", std::ios::out | std::ios::binary);
  // ofs_b.write(vec[1].data(), vec[1].size());
  // ofs_b.close();
  // std::cout << "**************vec[0]**************" << std::endl;
  // std::cout << vec[0] << std::endl;
  // std::cout << "**************vec[1]**************" << std::endl;
  // std::cout << vec[1] << std::endl;
  auto result = lwe->decryptGPU(secretA, vec[0], vec[1], progress);
  // std::cout << "**************result**************" << std::endl;
  // std::cout << result << std::endl;
  assert(msg == result);
  TIMERSTOP(multi)
  TIMERSTOP(total)
}

/**
 * @brief
 *  50 iters
 *  1 in  halfQ functional          492.321s
 *  1 out halfQ functional          482.413s
 *  2 out halfQ functional          361.522s  加速比 1.334  471.598s
 *  4 out halfQ functional          277.721s  加速比 1.737
 *  4 out halfQ lambda              294.037s  加速比 1.64
 *  8 out halfQ functional          300.733s  加速比 1.60
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char **argv) {
  // InitLog(shannonnet::LOG_NAME);
  // LOG(INFO) << "Hello World";
  // LOG(WARNING) << "Hello World";
  // test03();
  // std::cout << sizeof(long uint) << std::endl;
  // typedef uint16_t T;
  // std::cout << sizeof(T) << std::endl;
  // time_t startTimestamp = time(nullptr);
  // shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());
  // // lwe->generate_secret_file();
  // time_t endTimestamp = time(nullptr);
  // std::cout << endTimestamp - startTimestamp << std::endl;
  // std::cout << getpid() << std::endl;
  // std::cout << gettid() << std::endl;
  TIMERSTART(main)
  // test_gpu();
  for (size_t i = 0; i < 50; ++i) {
    // test_encrypt_gpu(i);
    test_gpu(i);
  }
  TIMERSTOP(main)
  return 0;
}