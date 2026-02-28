#pragma once
#include "../spdlog.h"

namespace spdlog {
namespace sinks {
class daily_file_sink_mt : public sink {
public:
  daily_file_sink_mt(const std::string &, int, int) {}
};
} // namespace sinks
} // namespace spdlog
