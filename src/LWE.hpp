#ifndef __SHANNON_NET_LWE_H__
#define __SHANNON_NET_LWE_H__

#include <bitset>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <oatpp/network/Server.hpp>

#include "common.h"

namespace shannonnet {
template <typename T, uint low, uint high>
T generate_random(T value) {
  static std::default_random_engine e(time(0));
  static std::uniform_int_distribution<T> N(low, high);
  return N(e);
}

template <typename T, uint low, uint high>
T generate_random_half(T value) {
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

  LWE(const std::string &secret_file = SECRET_A_PATH) : m_secret_file(secret_file) {}

  std::vector<std::string> encrypt(const std::string &in_message);
  std::string decrypt(const std::string &in_s, const std::string &in_b);

  // temporary function
  void generate_secret_file() { generate_secret(M, N, false, m_secret_file); }

  // temporary function
  std::string generate_secret_http(const uint64_t m, const uint64_t n) {
    auto vec = generate_secret(m, n);
    return {(char *)vec.data(), m * n * sizeof(T)};
  }

 private:
  std::vector<T> generate_secret(const uint64_t m, const uint64_t n, bool half = false,
                                 const std::string &out_file = "");
  std::vector<T> generate_secret_A(const std::string &file);
  std::vector<T> message2bin(const std::string &message);

 private:
  std::string m_secret_file;
};

template <typename T>
std::vector<T> LWE<T>::generate_secret(const uint64_t m, const uint64_t n, bool half, const std::string &out_file) {
  std::vector<T> out;
  out.reserve(m * n);
  std::function<T(T)> func;
  constexpr uint low = 0;
  if (half) {
    constexpr uint high = std::ceil(Q / 2) - 1;
    func = generate_random_half<T, low, high>;
  } else {
    constexpr uint high = Q - 1;
    func = generate_random<T, low, high>;
  }
  Eigen::Map<MatrixXT> mat(out.data(), m, n);
  mat = mat.unaryExpr(func);

  if (!out_file.empty()) {
    std::string str_out((char *)mat.data(), mat.size() * sizeof(T));
    std::ofstream ofs(out_file, std::ios::out | std::ios::binary | std::ios::ate);
    if (!ofs.is_open()) {
      LOG(ERROR) << "file open failed: " << out_file << ".";
      return {};
    }
    ofs << str_out;
    ofs.flush();
    ofs.close();
  }
  return out;
}

template <typename T>
std::vector<T> LWE<T>::generate_secret_A(const std::string &file) {
  std::vector<T> out;
  out.reserve(M * N);
  std::ifstream ifs(file, std::ios::in | std::ios::binary);
  if (!ifs.is_open()) {
    LOG(ERROR) << "file open failed: " << file << ".";
    return {};
  }
  ifs.read((char *)out.data(), M * N * sizeof(T));
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
std::vector<std::string> LWE<T>::encrypt(const std::string &in_message) {
  LOG_ASSERT(in_message.size() != 0) << "in_message size error";

  std::vector<T> vec_A = generate_secret_A(m_secret_file);
  Eigen::Map<MatrixXT> A(vec_A.data(), M, N);

  std::vector<T> vec_s = generate_secret(N, 1);
  Eigen::Map<MatrixXT> s(vec_s.data(), N, 1);

  std::vector<T> vec_e = generate_secret(M, 1, true);
  Eigen::Map<MatrixXT> e(vec_e.data(), M, 1);

  std::vector<T> vec_m = message2bin(in_message);
  Eigen::Map<MatrixXT> ms(vec_m.data(), M, 1);

  MatrixXT b(M, 1);
  std::function<T(T)> func = mod<T, Q>;
  b = ((A * s).unaryExpr(func) + e + (ms * (T)(std::ceil(Q / 2)))).unaryExpr(func);

  return {{(char *)s.data(), N * sizeof(T)}, {(char *)b.data(), M * sizeof(T)}};
}

template <typename T>
std::string LWE<T>::decrypt(const std::string &in_s, const std::string &in_b) {
  // 私钥
  std::vector<T> vec_A = generate_secret_A(m_secret_file);
  Eigen::Map<MatrixXT> A(vec_A.data(), M, N);

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
  for (auto &item : vec_res) {
    uint bit = 1;
    if (item < uint(Q / 2)) {
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