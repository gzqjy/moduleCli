#include "utils.h"
#include "picosha2.h"

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
namespace bp = boost::process;
constexpr const char* kTokenSalt = "4c61a9e9-bd52-40c8-91d3-5d37776e687d";
constexpr long long kTokenWindowSize = 60 * 60;
}

std::string sha256_hex(const std::string& input) {
    return picosha2::hash256_hex_string(input);
}

std::string basename_of(const std::string& path) {
    const std::size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string get_executable_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string full_path(path);
    return full_path.substr(0, full_path.find_last_of("\\/"));
#else
    char path[4096];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    if (count != -1) {
        std::string full_path(path, count);
        return full_path.substr(0, full_path.find_last_of("/"));
    }
    return ".";
#endif
}

std::string resolve_module_binary(const std::string& module_name) {
    if (module_name.find_first_of("/\\") != std::string::npos) {
        return module_name;
    }

    const auto resolved = boost::process::search_path(module_name);
    return resolved.empty() ? module_name : resolved.string();
}

std::string generate_current_token() {
    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / kTokenWindowSize;
    return sha256_hex(std::to_string(current_window) + ":" + kTokenSalt);
}

bool verify_token(const std::string& user_token) {
    const auto now_sec =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const long long current_window = now_sec / kTokenWindowSize;

    const auto check = [&](long long win) { return user_token == sha256_hex(std::to_string(win) + ":" + kTokenSalt); };

    return check(current_window) || check(current_window - 1);
}
