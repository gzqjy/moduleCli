#ifndef MY_SPDLOG_LOG_HPP
#define MY_SPDLOG_LOG_HPP

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

class MyLog {
public:
  static MyLog &instance() {
    static MyLog m_instance;
    return m_instance;
  }

  std::shared_ptr<spdlog::logger> getLogger() const { return logger_; }

  void init(const std::string &info_path, const int rotation_size, const short rotation_files) {
    if (initialized_) {
      return;
    }
    info_file_path_ = info_path;
    error_file_path_ = info_path + "-error";
    rotation_size_ = rotation_size;
    rotation_files_ = rotation_files;
    initLogger();
    initialized_ = true;
  }

  void setLogLevelImpl(const spdlog::level::level_enum level) const {
    if (sink_) {
      sink_->set_level(level);
    }
    if (console_sink_) {
      console_sink_->set_level(level);
    }
  }

  static void setLogLevel(const std::string &level) {
    spdlog::level::level_enum spdlog_level;

    if (level == "debug")
      spdlog_level = spdlog::level::debug;
    else if (level == "warn")
      spdlog_level = spdlog::level::warn;
    else if (level == "err")
      spdlog_level = spdlog::level::err;
    else if (level == "critical")
      spdlog_level = spdlog::level::critical;
    else if (level == "off")
      spdlog_level = spdlog::level::off;
    else
      spdlog_level = spdlog::level::info;

    spdlog::default_logger()->set_level(spdlog_level);
    MyLog::instance().setLogLevelImpl(spdlog_level);
  }

private:
  MyLog() {}

  ~MyLog() {
    if (logger_) {
      logger_->flush();
    }
  }

  void initLogger() {
    const auto logger = spdlog::get("multi_sink");
    if (!logger) {
      sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(info_file_path_, rotation_size_, rotation_files_);
      error_sink_ =
          std::make_shared<spdlog::sinks::rotating_file_sink_mt>(error_file_path_, rotation_size_, rotation_files_);
      console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

      sink_->set_level(spdlog::level::info);
      error_sink_->set_level(spdlog::level::err);
      console_sink_->set_level(spdlog::level::debug);

      sinks_ = {sink_, error_sink_, console_sink_};

      logger_ = std::make_shared<spdlog::logger>("multi_sink", sinks_.begin(), sinks_.end());
      logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] [%s:%#] [%P] - %v");
      logger_->flush_on(spdlog::level::info);
      logger_->set_level(spdlog::level::debug);

      spdlog::register_logger(logger_);
      spdlog::set_default_logger(logger_);
      spdlog::flush_every(std::chrono::seconds(10));
    }
  }

  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> sink_;
  std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> error_sink_;
  std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
  std::vector<spdlog::sink_ptr> sinks_;
  std::string info_file_path_;
  std::string error_file_path_;
  int rotation_size_{};
  short int rotation_files_{};
  bool initialized_{};
};

#define ilog MyLog::instance().getLogger()
#define MYLOG_RESET(x, y, z) MyLog::instance().init(x, y, z)
#define MYLOG_LOG_LEVEL(x) MyLog::instance().setLogLevel(x)

#endif // MY_SPDLOG_LOG_HPP
