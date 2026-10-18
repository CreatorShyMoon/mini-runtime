#pragma once

#include "ctr/config.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ctr {

enum class ParseStatus { Ok, Help, Error };

struct ParseResult {
    ParseStatus status{ParseStatus::Error};
    Config config;
    std::string error;
};

// Parse `run [options] <command> [args...]`. Pure, allocation-bounded and
// fuzzable: no syscalls, no globals.
ParseResult parse_args(const std::vector<std::string_view>& args);

// Parse a size such as "64m", "1G", "512k" or a raw byte count.
// Returns std::nullopt on malformed input or overflow.
std::optional<std::uint64_t> parse_size(std::string_view text);

// Usage text printed for --help and on argument errors.
std::string usage();

} // namespace ctr
