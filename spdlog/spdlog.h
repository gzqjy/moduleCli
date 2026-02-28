#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace spdlog {
namespace level {
enum level_enum { trace, debug, info, warn, err, critical, off };
}

namespace sinks {
class sink {
public:
  virtual ~sink() {}
  virtual void set_level(level::level_enum lvl) { level_ = lvl; }
  virtual void log(level::level_enum, const std::string &) {}

protected:
  level::level_enum level_ = level::trace;
};
} // namespace sinks

using sink_ptr = std::shared_ptr<sinks::sink>;

inline std::string format_string(const std::string &fmt) { return fmt; }

template <typename T, typename... Args>
std::string format_string(const std::string &fmt, T value, Args... args) {
  const std::size_t pos = fmt.find("{}");
  if (pos == std::string::npos) {
    return fmt;
  }
  std::ostringstream oss;
  oss << fmt.substr(0, pos) << value << format_string(fmt.substr(pos + 2), args...);
  return oss.str();
}

class logger {
public:
  logger(const std::string &name, std::vector<sink_ptr>::iterator begin, std::vector<sink_ptr>::iterator end)
      : name_(name), sinks_(begin, end) {}

  void set_pattern(const std::string &) {}
  void flush_on(level::level_enum) {}
  void flush() {}
  void set_level(level::level_enum lvl) { level_ = lvl; }

  template <typename... Args>
  void info(const std::string &fmt, Args... args) {
    log(level::info, format_string(fmt, args...));
  }

  template <typename... Args>
  void error(const std::string &fmt, Args... args) {
    log(level::err, format_string(fmt, args...));
  }

private:
  void log(level::level_enum lvl, const std::string &msg) {
    if (lvl < level_) {
      return;
    }
    for (std::size_t i = 0; i < sinks_.size(); ++i) {
      sinks_[i]->log(lvl, msg);
    }
  }

  std::string name_;
  std::vector<sink_ptr> sinks_;
  level::level_enum level_ = level::trace;
};

inline std::shared_ptr<logger> &default_logger_storage() {
  static std::shared_ptr<logger> lg;
  return lg;
}

inline std::map<std::string, std::shared_ptr<logger> > &registry() {
  static std::map<std::string, std::shared_ptr<logger> > r;
  return r;
}

inline std::shared_ptr<logger> get(const std::string &name) {
  std::map<std::string, std::shared_ptr<logger> >::iterator it = registry().find(name);
  if (it == registry().end()) {
    return std::shared_ptr<logger>();
  }
  return it->second;
}

inline void register_logger(const std::shared_ptr<logger> &lg) { registry()["multi_sink"] = lg; }
inline void set_default_logger(const std::shared_ptr<logger> &lg) { default_logger_storage() = lg; }

inline std::shared_ptr<logger> default_logger() { return default_logger_storage(); }

inline void flush_every(std::chrono::seconds) {}

} // namespace spdlog

#define SPDLOG_INFO(...) ::spdlog::default_logger()->info(__VA_ARGS__)
#define SPDLOG_ERROR(...) ::spdlog::default_logger()->error(__VA_ARGS__)
#define SPDLOG_LEVEL_TRACE 0
