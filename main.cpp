#include <chrono>
#include <iomanip>
#include <iostream>
#include <openssl/sha.h>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string sha256_hex(const std::string &input) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.size(), hash);

  std::ostringstream oss;
  for (unsigned char byte : hash) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return oss.str();
}

bool verify_token(const std::string &user_token) {
  const std::string salt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
  const long long window_size = 60;

  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  const long long current_window = seconds / window_size;
  const long long prev_window = current_window - 1;

  const std::string current_expected = sha256_hex(std::to_string(current_window) + salt);
  const std::string prev_expected = sha256_hex(std::to_string(prev_window) + salt);

  return user_token == current_expected || user_token == prev_expected;
}

bool run_module_command(const std::string &module_name, const std::string &command,
                        const std::vector<std::string> &extra_args) {
  std::cout << "module=" << module_name << " command=" << command;
  if (!extra_args.empty()) {
    std::cout << " args=[";
    for (size_t i = 0; i < extra_args.size(); ++i) {
      std::cout << extra_args[i];
      if (i + 1 != extra_args.size()) {
        std::cout << ", ";
      }
    }
    std::cout << "]";
  }
  std::cout << '\n';

  if (command == "start") {
    std::cout << "Start module: " << module_name << '\n';
    return true;
  }
  if (command == "stop") {
    std::cout << "Stop module: " << module_name << '\n';
    return true;
  }
  if (command == "preinst") {
    std::cout << "Run pre-install actions for module: " << module_name << '\n';
    return true;
  }
  if (command == "postinst") {
    std::cout << "Run post-install actions for module: " << module_name << '\n';
    return true;
  }
  if (command == "preun") {
    std::cout << "Run pre-uninstall actions for module: " << module_name << '\n';
    return true;
  }
  if (command == "postun") {
    std::cout << "Run post-uninstall actions for module: " << module_name << '\n';
    return true;
  }

  std::cerr << "Unsupported command: " << command
            << " (supported: start|stop|preinst|postinst|preun|postun)\n";
  return false;
}

void print_usage(const char *prog) {
  std::cerr << "Usage: " << prog
            << " <module_name> <command> <magicnum(user_token)> [arg1] [arg2]\n";
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc < 4) {
    print_usage(argv[0]);
    return 1;
  }

  const std::string module_name = argv[1];
  const std::string command = argv[2];
  const std::string user_token = argv[3];

  std::vector<std::string> extra_args;
  for (int i = 4; i < argc; ++i) {
    extra_args.emplace_back(argv[i]);
  }

  if (!verify_token(user_token)) {
    std::cerr << "Token verification failed.\n";
    return 2;
  }

  if (!run_module_command(module_name, command, extra_args)) {
    return 3;
  }

  return 0;
}
