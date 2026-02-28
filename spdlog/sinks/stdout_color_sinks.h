#pragma once
#include "../spdlog.h"

namespace spdlog {
namespace sinks {
class stdout_color_sink_mt : public sink {
public:
  void log(level::level_enum lvl, const std::string &msg) {
    if (lvl < level_) {
      return;
    }
    std::cout << msg << std::endl;
  }
};
} // namespace sinks
} // namespace spdlog
