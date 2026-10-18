#include "ctr/logger.hpp"

#include <atomic>
#include <cstdio>
#include <string_view>

namespace ctr {
namespace {

std::atomic<LogLevel> g_level{LogLevel::Info};

std::string_view level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

} // namespace

void set_log_level(LogLevel level) {
    g_level.store(level, std::memory_order_relaxed);
}

void log(LogLevel level, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(g_level.load(std::memory_order_relaxed))) {
        return;
    }
    std::fprintf(stderr, "[mini-ctr] %.*s: %.*s\n", static_cast<int>(level_name(level).size()),
                 level_name(level).data(), static_cast<int>(message.size()), message.data());
}

} // namespace ctr
