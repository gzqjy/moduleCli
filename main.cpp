#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "logger.h"
#include "picosha2.h"

namespace {

std::string sha256_hex(const std::string &input) {
  return picosha2::hash256_hex_string(input);
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
