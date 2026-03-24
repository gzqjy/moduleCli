#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.h"

namespace {

class Sha256 {
public:
  Sha256() { reset(); }

  void update(const std::string &input) {
    const auto *data = reinterpret_cast<const std::uint8_t *>(input.c_str());
    const size_t len = input.size();
    for (size_t i = 0; i < len; ++i) {
      data_[datalen_++] = data[i];
      if (datalen_ == 64) {
        transform();
        bitlen_ += 512;
        datalen_ = 0;
      }
    }
  }

  std::string final_hex() {
    std::uint64_t i = datalen_;
    if (datalen_ < 56) {
      data_[i++] = 0x80;
      while (i < 56) {
        data_[i++] = 0x00;
      }
    } else {
      data_[i++] = 0x80;
      while (i < 64) {
        data_[i++] = 0x00;
      }
      transform();
      std::memset(data_, 0, 56);
    }

    bitlen_ += static_cast<std::uint64_t>(datalen_) * 8;
    for (int j = 0; j < 8; ++j) {
      data_[63 - j] = static_cast<std::uint8_t>(bitlen_ >> (j * 8));
    }
    transform();

    char buf[65] = {0};
    for (int j = 0; j < 8; ++j) {
      std::snprintf(buf + j * 8, 9, "%08x", state_[j]);
    }
    return std::string(buf);
  }

private:
  static inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (~x & z); }
  static inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static inline std::uint32_t ep0(std::uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static inline std::uint32_t ep1(std::uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static inline std::uint32_t sig0(std::uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static inline std::uint32_t sig1(std::uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

  void reset() {
    datalen_ = 0;
    bitlen_ = 0;
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
  }

  void transform() {
    static const std::uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};

    std::uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
      m[i] = (std::uint32_t(data_[j]) << 24) | (std::uint32_t(data_[j + 1]) << 16) | (std::uint32_t(data_[j + 2]) << 8) |
             std::uint32_t(data_[j + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
    }

    std::uint32_t v[8];
    std::memcpy(v, state_, sizeof(v));

    for (int i = 0; i < 64; ++i) {
      const std::uint32_t t1 = v[7] + ep1(v[4]) + ch(v[4], v[5], v[6]) + k[i] + m[i];
      const std::uint32_t t2 = ep0(v[0]) + maj(v[0], v[1], v[2]);
      v[7] = v[6];
      v[6] = v[5];
      v[5] = v[4];
      v[4] = v[3] + t1;
      v[3] = v[2];
      v[2] = v[1];
      v[1] = v[0];
      v[0] = t1 + t2;
    }

    for (int i = 0; i < 8; ++i) {
      state_[i] += v[i];
    }
  }

  std::uint8_t data_[64];
  std::uint32_t datalen_;
  std::uint64_t bitlen_;
  std::uint32_t state_[8];
};

std::string sha256_hex(const std::string &input) {
  Sha256 ctx;
  ctx.update(input);
  return ctx.final_hex();
}

void print_usage(const char *program_name) {
  std::cout << "Usage: " << program_name
            << " <module_name> <command> <magicnum> [extra_args...]\n"
               "Commands: start | stop | preinst | postinst | preun | postun"
            << std::endl;
}

bool verify_token(const std::string &user_token) {
  const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
  const long long window_size = 60;

  const auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const long long current_window = now_sec / window_size;

  const auto check = [&](long long win) { return user_token == sha256_hex(std::to_string(win) + ":" + salt); };

  return check(current_window) || check(current_window - 1);
}

bool handle_commands(const std::string &module_name, const std::string &command,
                     const std::vector<std::string> &extra_args) {
  static const std::unordered_map<std::string, std::string> cmd_desc = {{"start", "Start module"},
                                                                         {"stop", "Stop module"},
                                                                         {"preinst", "Run pre-install actions"},
                                                                         {"postinst", "Run post-install actions"},
                                                                         {"preun", "Run pre-uninstall actions"},
                                                                         {"postun", "Run post-uninstall actions"}};

  const auto it = cmd_desc.find(command);
  if (it == cmd_desc.end()) {
    SPDLOG_ERROR("Unsupported command: {}", command);
    return false;
  }

  SPDLOG_INFO("{}: {} (Args count: {})", it->second, module_name, extra_args.size());
  return true;
}

} // namespace

int main(int argc, char *argv[]) {
  MYLOG_RESET("moduleCli.log", 1024 * 1024, 3);
  MYLOG_LOG_LEVEL("debug");

  // 1. 基础校验 (必须包含 module_name command magicnum)
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  // 2. 直接提取位置参数
  std::string module_name = argv[1];
  std::string command = argv[2];
  std::string magicnum = argv[3];

  // 3. 提取剩余的 arg1, arg2...
  std::vector<std::string> extra_args;
  for (int i = 4; i < argc; ++i) {
    extra_args.push_back(argv[i]);
  }

  // 4. Token 校验
  if (!verify_token(magicnum)) {
    SPDLOG_ERROR("Token 校验失败！");
    return 2;
  }

  // 5. 执行命令逻辑
  if (!handle_commands(module_name, command, extra_args)) {
    return 3;
  }

  SPDLOG_INFO("Command executed successfully.");
  return 0;
}
