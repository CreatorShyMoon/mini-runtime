#include "ctr/cli.hpp"

#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>

namespace ctr {
namespace {

bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

std::optional<std::uint64_t> parse_unsigned(std::string_view digits) {
    if (digits.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (char c : digits) {
        if (!is_digit(c)) {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

std::optional<double> parse_double(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::string owned(text);
    char* end = nullptr;
    const double value = std::strtod(owned.c_str(), &end);
    if (end == owned.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<std::uint64_t> parse_size(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint64_t multiplier = 1;
    std::string_view digits = text;
    switch (text.back()) {
    case 'k':
    case 'K':
        multiplier = 1024ULL;
        digits = text.substr(0, text.size() - 1);
        break;
    case 'm':
    case 'M':
        multiplier = 1024ULL * 1024;
        digits = text.substr(0, text.size() - 1);
        break;
    case 'g':
    case 'G':
        multiplier = 1024ULL * 1024 * 1024;
        digits = text.substr(0, text.size() - 1);
        break;
    case 'b':
    case 'B':
        digits = text.substr(0, text.size() - 1);
        break;
    default:
        break;
    }

    const auto value = parse_unsigned(digits);
    if (!value) {
        return std::nullopt;
    }
    if (*value > std::numeric_limits<std::uint64_t>::max() / multiplier) {
        return std::nullopt;
    }
    return *value * multiplier;
}

ParseResult parse_args(const std::vector<std::string_view>& args) {
    ParseResult result;

    if (args.empty()) {
        result.error = "missing subcommand (expected 'run')";
        return result;
    }
    if (args.front() == "-h" || args.front() == "--help" || args.front() == "help") {
        result.status = ParseStatus::Help;
        return result;
    }
    if (args.front() != "run") {
        result.error = "unknown subcommand: " + std::string(args.front());
        return result;
    }

    Config config;
    bool saw_command = false;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view token = args[i];
        if (token.empty()) {
            continue;
        }
        if (token == "-h" || token == "--help") {
            result.status = ParseStatus::Help;
            return result;
        }
        if (token.size() < 2 || token.compare(0, 2, "--") != 0) {
            saw_command = true;
            for (; i < args.size(); ++i) {
                config.command.emplace_back(args[i]);
            }
            break;
        }

        std::string_view name = token.substr(2);
        std::string_view value;
        bool has_value = false;
        const auto eq = name.find('=');
        if (eq != std::string_view::npos) {
            value = name.substr(eq + 1);
            name = name.substr(0, eq);
            has_value = true;
        }

        const auto take_value = [&](std::string_view& out) -> bool {
            if (has_value) {
                out = value;
                return true;
            }
            if (i + 1 >= args.size()) {
                return false;
            }
            out = args[++i];
            return true;
        };

        std::string_view raw;
        if (name == "rootfs") {
            if (!take_value(raw)) {
                result.error = "--rootfs requires a value";
                return result;
            }
            config.rootfs = std::string(raw);
        } else if (name == "hostname") {
            if (!take_value(raw)) {
                result.error = "--hostname requires a value";
                return result;
            }
            config.hostname = std::string(raw);
        } else if (name == "memory") {
            if (!take_value(raw)) {
                result.error = "--memory requires a value";
                return result;
            }
            const auto bytes = parse_size(raw);
            if (!bytes) {
                result.error = "invalid --memory value: " + std::string(raw);
                return result;
            }
            config.memory_bytes = *bytes;
        } else if (name == "pids") {
            if (!take_value(raw)) {
                result.error = "--pids requires a value";
                return result;
            }
            const auto count = parse_size(raw);
            if (!count) {
                result.error = "invalid --pids value: " + std::string(raw);
                return result;
            }
            config.pids_limit = *count;
        } else if (name == "cpus") {
            if (!take_value(raw)) {
                result.error = "--cpus requires a value";
                return result;
            }
            const auto cpus = parse_double(raw);
            if (!cpus) {
                result.error = "invalid --cpus value: " + std::string(raw);
                return result;
            }
            config.cpus = *cpus;
        } else if (name == "keep-caps") {
            config.drop_capabilities = false;
        } else if (name == "no-cgroups") {
            config.use_cgroups = false;
        } else if (name == "host-pid") {
            config.new_pid_namespace = false;
        } else if (name == "net") {
            config.new_network_namespace = true;
        } else {
            result.error = "unknown option: --" + std::string(name);
            return result;
        }
    }

    if (!saw_command) {
        result.error = "no command specified";
        return result;
    }

    const std::string validation = validate_config(config);
    if (!validation.empty()) {
        result.error = validation;
        return result;
    }

    result.config = std::move(config);
    result.status = ParseStatus::Ok;
    return result;
}

std::string usage() {
    return "Usage: mini-ctr run [options] <command> [args...]\n"
           "\n"
           "Options:\n"
           "  --rootfs <dir>     root filesystem to enter (default: ./rootfs)\n"
           "  --hostname <name>  container hostname (default: container)\n"
           "  --memory <size>    memory limit, e.g. 64m, 1g\n"
           "  --pids <n>         maximum number of processes\n"
           "  --cpus <n>         CPU quota in cores, e.g. 0.5\n"
           "  --keep-caps        do not drop capabilities\n"
           "  --no-cgroups        run without resource limits\n"
           "  --host-pid          share the host PID namespace\n"
           "  --net               create a new network namespace\n"
           "  -h, --help          show this help\n";
}

} // namespace ctr
