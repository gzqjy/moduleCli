#pragma once

#include <string>
#include <vector>

std::string sha256_hex(const std::string& input);
std::string basename_of(const std::string& path);
std::string get_executable_dir();
std::string resolve_module_binary(const std::string& module_name);

// Token functions
std::string generate_current_token();
bool verify_token(const std::string& user_token);
