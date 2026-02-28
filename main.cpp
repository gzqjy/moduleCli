#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cmdline.h"
#include "logger.h"

namespace {

class Sha256 {
public:
  Sha256() { reset(); }

  void update(const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      data_[datalen_++] = data[i];
      if (datalen_ == 64) {
        transform();
        bitlen_ += 512;
        datalen_ = 0;
      }
    }
  }

  void update(const std::string &input) {
    update(reinterpret_cast<const unsigned char *>(input.c_str()), input.size());
  }

  std::string final_hex() {
    unsigned int i = datalen_;

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
    data_[63] = static_cast<unsigned char>(bitlen_);
    data_[62] = static_cast<unsigned char>(bitlen_ >> 8);
    data_[61] = static_cast<unsigned char>(bitlen_ >> 16);
    data_[60] = static_cast<unsigned char>(bitlen_ >> 24);
    data_[59] = static_cast<unsigned char>(bitlen_ >> 32);
    data_[58] = static_cast<unsigned char>(bitlen_ >> 40);
    data_[57] = static_cast<unsigned char>(bitlen_ >> 48);
    data_[56] = static_cast<unsigned char>(bitlen_ >> 56);
    transform();

    std::ostringstream oss;
    for (int j = 0; j < 8; ++j) {
      oss << std::hex << std::setw(8) << std::setfill('0') << state_[j];
    }
    return oss.str();
  }

private:
  static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (~x & z); }
  static std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static std::uint32_t ep0(std::uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static std::uint32_t ep1(std::uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static std::uint32_t sig0(std::uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static std::uint32_t sig1(std::uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

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
      m[i] = (static_cast<std::uint32_t>(data_[j]) << 24) | (static_cast<std::uint32_t>(data_[j + 1]) << 16) |
             (static_cast<std::uint32_t>(data_[j + 2]) << 8) | static_cast<std::uint32_t>(data_[j + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (int i = 0; i < 64; ++i) {
      std::uint32_t t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
      std::uint32_t t2 = ep0(a) + maj(a, b, c);
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  unsigned char data_[64];
  std::uint32_t datalen_;
  std::uint64_t bitlen_;
  std::uint32_t state_[8];
};

std::string sha256_hex(const std::string &input) {
  Sha256 ctx;
  ctx.update(input);
  return ctx.final_hex();
}

bool verify_token(const std::string &user_token) {
  const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
  const long long window_size = 60;

  const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  const std::chrono::seconds duration = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
  const long long seconds = duration.count();
  const long long current_window = seconds / window_size;
  const long long prev_window = current_window - 1;

  const std::string current_expected = sha256_hex(std::to_string(current_window) + salt);
  const std::string prev_expected = sha256_hex(std::to_string(prev_window) + salt);

  return user_token == current_expected || user_token == prev_expected;
}

bool run_module_command(const std::string &module_name, const std::string &command,
                        const std::vector<std::string> &extra_args) {
  std::ostringstream args_msg;
  args_msg << "module=" << module_name << " command=" << command;
  if (!extra_args.empty()) {
    args_msg << " args=[";
    for (size_t i = 0; i < extra_args.size(); ++i) {
      args_msg << extra_args[i];
      if (i + 1 != extra_args.size()) {
        args_msg << ", ";
      }
    }
    args_msg << "]";
  }
  SPDLOG_INFO("{}", args_msg.str());

  if (command == "start") {
    SPDLOG_INFO("Start module: {}", module_name);
    return true;
  }
  if (command == "stop") {
    SPDLOG_INFO("Stop module: {}", module_name);
    return true;
  }
  if (command == "preinst") {
    SPDLOG_INFO("Run pre-install actions for module: {}", module_name);
    return true;
  }
  if (command == "postinst") {
    SPDLOG_INFO("Run post-install actions for module: {}", module_name);
    return true;
  }
  if (command == "preun") {
    SPDLOG_INFO("Run pre-uninstall actions for module: {}", module_name);
    return true;
  }
  if (command == "postun") {
    SPDLOG_INFO("Run post-uninstall actions for module: {}", module_name);
    return true;
  }

  SPDLOG_ERROR("Unsupported command: {} (supported: start|stop|preinst|postinst|preun|postun)", command);
  return false;
}

std::vector<std::string> normalize_args(int argc, char *argv[]) {
  std::vector<std::string> args;

  if (argc >= 4 && argv[1][0] != '-') {
    args.push_back(argv[0]);
    args.push_back("--module_name");
    args.push_back(argv[1]);
    args.push_back("--command");
    args.push_back(argv[2]);
    args.push_back("--magicnum");
    args.push_back(argv[3]);
    for (int i = 4; i < argc; ++i) {
      args.push_back(argv[i]);
    }
    return args;
  }

  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return args;
}

} // namespace

int main(int argc, char *argv[]) {
  MYLOG_RESET("moduleCli.log", 1024 * 1024, 3);
  MYLOG_LOG_LEVEL("debug");
  if (argc == 1) {
    cmdline::parser help_parser;
    help_parser.set_program_name(argv[0]);
    help_parser.footer("[args...]");
    help_parser.add<std::string>("module_name", 'm', "module name", true);
    help_parser.add<std::string>("command", 'c', "module command", true);
    help_parser.add<std::string>("magicnum", 't', "user token", true);
    SPDLOG_ERROR("\n{}", help_parser.usage());
    return 1;
  }

  if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    cmdline::parser help_parser;
    help_parser.set_program_name(argv[0]);
    help_parser.footer("[args...]");
    help_parser.add<std::string>("module_name", 'm', "module name", true);
    help_parser.add<std::string>("command", 'c', "module command", true);
    help_parser.add<std::string>("magicnum", 't', "user token", true);
    SPDLOG_INFO("\n{}", help_parser.usage());
    return 0;
  }

  cmdline::parser parser;
  parser.set_program_name(argv[0]);
  parser.footer("[args...]");
  parser.add<std::string>("module_name", 'm', "module name", true);
  parser.add<std::string>("command", 'c', "module command", true, std::string(),
                          cmdline::oneof<std::string>("start", "stop", "preinst", "postinst", "preun", "postun"));
  parser.add<std::string>("magicnum", 't', "user token", true);

  const std::vector<std::string> normalized = normalize_args(argc, argv);
  std::vector<const char *> cargs(normalized.size());
  for (size_t i = 0; i < normalized.size(); ++i) {
    cargs[i] = normalized[i].c_str();
  }

  if (!parser.parse(static_cast<int>(cargs.size()), &cargs[0])) {
    SPDLOG_ERROR("{}\n{}", parser.error(), parser.usage());
    return 1;
  }

  const std::string module_name = parser.get<std::string>("module_name");
  const std::string command = parser.get<std::string>("command");
  const std::string user_token = parser.get<std::string>("magicnum");

  if (!verify_token(user_token)) {
    SPDLOG_ERROR("Token verification failed.");
    return 2;
  }

  if (!run_module_command(module_name, command, parser.rest())) {
    return 3;
  }

  return 0;
}
