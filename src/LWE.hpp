#ifndef __SHANNON_NET_LWE_H__
#define __SHANNON_NET_LWE_H__

#include <bitset>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "crc32c/crc32c.h"
#include <Eigen/Dense>
#include <oatpp/network/Server.hpp>

#include "src/common.h"

namespace shannonnet {
template <typename T, uint low, uint high>
T generateRandom(T value) {
  static std::default_random_engine e(time(0));
  static std::uniform_int_distribution<T> N(low, high);
  return N(e);
}

template <typename T, uint low, uint high>
T generateRandomHalf(T value) {
  static std::default_random_engine e(time(0));
  static std::uniform_int_distribution<T> N(low, high);
  return N(e);
}

template <typename T, uint Q>
T mod(T value) {
  return value % Q;
}

template <typename T>
class LWE {
 public:
  typedef std::shared_ptr<LWE> ptr;

  using MatrixXT = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

  LWE() {}
  ~LWE() {}

  std::vector<T> generateSecretA(const std::string &secretFile);
  std::vector<std::string> encrypt(const std::string &inMessage, const std::vector<T> &vec_A);
  std::string decrypt(const std::vector<T> &vec_A, const std::string &in_s, const std::string &in_b);

  // temporary function
  // generate waited secret
  void generateSecretFile(const std::string &waiting_secret_path, bool test = false) {
    uint32_t secretASize = SN_DEBUG ? DEBUG_SECRET_A_SIZE : SECRET_A_SIZE;
    generateSecret(secretASize / sizeof(S_Type), 1, false, waiting_secret_path);
  }

 private:
  std::vector<T> generateSecret(const uint32_t m, const uint32_t n, bool half = false, const std::string &outFile = "");
  std::vector<T> message2bin(const std::string &message);
};

template <typename T>
std::vector<T> LWE<T>::generateSecret(const uint32_t m, const uint32_t n, bool half, const std::string &outFile) {
  std::vector<T> out;
  out.reserve(m * n);
  std::function<T(T)> func;
  constexpr uint low = 0;
  if (half) {
    constexpr uint high = std::ceil(Q / 2) - 1;
    func = generateRandomHalf<T, low, high>;
  } else {
    constexpr uint high = Q - 1;
    func = generateRandom<T, low, high>;
  }
  Eigen::Map<MatrixXT> mat(out.data(), m, n);
  mat = mat.unaryExpr(func);

  if (!outFile.empty()) {
    auto crc32Val = crc32c::Crc32c(reinterpret_cast<char *>(mat.data()), mat.size() * sizeof(T) - CRC_LEN);
    std::stringstream ss;
    ss << std::setw(CRC_LEN) << std::setfill('0') << std::to_string(crc32Val);
    auto crcStr = ss.str();
    std::ofstream ofs(outFile, std::ios::out | std::ios::binary | std::ios::ate);
    if (!ofs.is_open()) {
      LOG(ERROR) << "File open failed, file: " + outFile;
      return {};
    }
    ofs.write(reinterpret_cast<char *>(mat.data()), mat.size() * sizeof(T) - CRC_LEN);
    ofs.write(crcStr.data(), crcStr.size());
    ofs.flush();
    ofs.close();
    LOG(INFO) << "LWE save secret success, file: " + outFile + ", crc32: " + crcStr;
  }
  return out;
}

template <typename T>
std::vector<T> LWE<T>::generateSecretA(const std::string &secretFile) {
  LOG_ASSERT(!secretFile.empty());
  std::vector<T> out(M * N);
  std::ifstream ifs(secretFile, std::ios::in | std::ios::binary);
  if (!ifs.is_open()) {
    LOG(ERROR) << "File open failed: " << secretFile << ".";
    return {};
  }
  ifs.read((char *)out.data(), M * N * sizeof(T));
  ifs.close();
  return out;
}

template <typename T>
std::vector<T> LWE<T>::message2bin(const std::string &message) {
  std::vector<T> bin_vec;
  bin_vec.reserve(message.size() * BIT);

  for (auto &c : message) {
    std::bitset<BIT> bs(c);
    for (int i = bs.size() - 1; i >= 0; --i) {
      bin_vec.emplace_back(bs[i]);
    }
  }
  return bin_vec;
}

template <typename T>
std::vector<std::string> LWE<T>::encrypt(const std::string &inMessage, const std::vector<T> &vec_A) {
  LOG_ASSERT(inMessage.size() != 0);
  Eigen::Map<MatrixXT> A(const_cast<T *>(vec_A.data()), M, N);

  std::vector<T> vec_s = generateSecret(N, 1);
  Eigen::Map<MatrixXT> s(vec_s.data(), N, 1);

  std::vector<T> vec_e = generateSecret(M, 1, true);
  Eigen::Map<MatrixXT> e(vec_e.data(), M, 1);

  std::vector<T> vec_m = message2bin(inMessage);
  Eigen::Map<MatrixXT> ms(vec_m.data(), M, 1);

  MatrixXT b(M, 1);
  std::function<T(T)> func = mod<T, Q>;

  T halfQ = (T)(std::ceil(Q / 2));
  b = ((A * s).unaryExpr(func) + e + ms * halfQ).unaryExpr(func);

  return {{(char *)s.data(), N * sizeof(T)}, {(char *)b.data(), M * sizeof(T)}};
}

template <typename T>
std::string LWE<T>::decrypt(const std::vector<T> &vec_A, const std::string &in_s, const std::string &in_b) {
  // 私钥
  Eigen::Map<MatrixXT> A(const_cast<T *>(vec_A.data()), M, N);

  // 公钥
  Eigen::Map<MatrixXT> s((T *)in_s.data(), N, 1);
  Eigen::Map<MatrixXT> b((T *)in_b.data(), M, 1);

  // 生成M个Q
  std::vector<T> vec_q(M, Q);
  Eigen::Map<MatrixXT> q_(vec_q.data(), M, 1);

  // 计算
  MatrixXT b_(M, 1);
  std::function<T(T)> func = mod<T, Q>;
  b_ = (b + q_ - (A * s).unaryExpr(func)).unaryExpr(func);

  std::vector<T> vec_res(b_.data(), b_.data() + b_.size());
  std::string cur_str;
  std::string out_message;
  T halfQ = Q / 2;
  for (auto &item : vec_res) {
    uint bit = 1;
    if (item < halfQ) {
      bit = 0;
    }

    cur_str += std::to_string(bit);
    if (cur_str.size() == BIT) {
      std::bitset<BIT> bs(cur_str);
      out_message += (char)(bs.to_ulong());
      cur_str.clear();
    }
  }
  return out_message;
}
}  // namespace shannonnet
#endif  // __SHANNON_NET_LWE_H__