#pragma once
#include "../spdlog.h"

namespace spdlog {
namespace sinks {
class rotating_file_sink_mt : public sink {
public:
  rotating_file_sink_mt(const std::string &, int, short) {}
};
} // namespace sinks
} // namespace spdlog
