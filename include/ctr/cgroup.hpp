#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>

namespace ctr {

// Minimal cgroups v2 (unified hierarchy) helper. Creates a child cgroup under
// the caller's own cgroup and exposes the limit setters we need.
class CGroup {
public:
    // True when a unified cgroups v2 mount is visible.
    static bool available();

    // Create `name` under the caller's cgroup. Returns std::nullopt when
    // cgroups v2 is unavailable or the cgroup cannot be created.
    static std::optional<CGroup> create(const std::string& name);

    CGroup(CGroup&& other) noexcept;
    CGroup& operator=(CGroup&& other) noexcept;
    CGroup(const CGroup&) = delete;
    CGroup& operator=(const CGroup&) = delete;
    ~CGroup();

    bool set_memory_limit(std::uint64_t bytes);
    bool set_pids_limit(std::uint64_t count);
    bool set_cpu_limit(double cpus);

    // Move `pid` into this cgroup (call it right after clone()).
    bool add_process(pid_t pid);

    // Remove the cgroup directory; idempotent.
    void cleanup();

    const std::string& path() const noexcept;

private:
    explicit CGroup(std::string path);

    std::string path_;
};

} // namespace ctr
