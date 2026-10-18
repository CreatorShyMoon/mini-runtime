#pragma once

#include <string_view>

namespace ctr {

enum class LogLevel { Debug, Info, Warn, Error };

void set_log_level(LogLevel level);
void log(LogLevel level, std::string_view message);

} // namespace ctr
