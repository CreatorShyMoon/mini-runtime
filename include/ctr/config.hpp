#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ctr {

// Everything the runtime needs to launch one container.
struct Config {
    std::string rootfs{"./rootfs"};
    std::string hostname{"container"};
    std::vector<std::string> command; // program + args

    std::optional<std::uint64_t> memory_bytes;
    std::optional<std::uint64_t> pids_limit;
    std::optional<double> cpus;

    bool drop_capabilities{true};
    bool use_cgroups{true};
    bool new_pid_namespace{true};
    bool new_ipc_namespace{true};
    bool new_network_namespace{false};
};

// Returns an empty string when the config is usable, otherwise a message.
std::string validate_config(const Config& config);

} // namespace ctr
