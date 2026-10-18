#include "ctr/config.hpp"

namespace ctr {

std::string validate_config(const Config& config) {
    if (config.rootfs.empty()) {
        return "rootfs path is empty";
    }
    if (config.hostname.empty()) {
        return "hostname is empty";
    }
    if (config.hostname.size() > 64) {
        return "hostname too long (max 64 bytes)";
    }
    if (config.command.empty()) {
        return "no command specified";
    }
    if (config.command.front().empty()) {
        return "command is empty";
    }
    if (config.memory_bytes && *config.memory_bytes == 0) {
        return "memory limit must be greater than zero";
    }
    if (config.pids_limit && *config.pids_limit == 0) {
        return "pids limit must be greater than zero";
    }
    if (config.cpus && *config.cpus <= 0.0) {
        return "cpus must be greater than zero";
    }
    return {};
}

} // namespace ctr
