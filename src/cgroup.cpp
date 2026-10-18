#include "ctr/cgroup.hpp"

#include <cerrno>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace ctr {
namespace {

constexpr const char* kMountPoint = "/sys/fs/cgroup";

bool write_file(const std::string& path, const std::string& value) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << value;
    return static_cast<bool>(out);
}

// The caller's own cgroup, relative to the unified mount point. The line in
// /proc/self/cgroup looks like "0::/user.slice/...".
std::string own_cgroup_path() {
    std::ifstream in("/proc/self/cgroup");
    std::string line;
    while (std::getline(in, line)) {
        const auto sep = line.find("::");
        if (sep != std::string::npos) {
            return line.substr(sep + 2);
        }
    }
    return "/";
}

} // namespace

bool CGroup::available() {
    std::ifstream in(std::string(kMountPoint) + "/cgroup.controllers");
    return static_cast<bool>(in);
}

std::optional<CGroup> CGroup::create(const std::string& name) {
    if (!available()) {
        return std::nullopt;
    }
    const std::string base = std::string(kMountPoint) + own_cgroup_path();
    const std::string path = base + "/" + name;
    if (::mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        return std::nullopt;
    }
    // Ask the parent cgroup to hand the controllers down to us.
    write_file(base + "/cgroup.subtree_control", "+memory +pids +cpu");
    return CGroup{path};
}

CGroup::CGroup(std::string path) : path_(std::move(path)) {}

CGroup::CGroup(CGroup&& other) noexcept : path_(std::move(other.path_)) {
    other.path_.clear();
}

CGroup& CGroup::operator=(CGroup&& other) noexcept {
    if (this != &other) {
        if (!path_.empty()) {
            ::rmdir(path_.c_str());
        }
        path_ = std::move(other.path_);
        other.path_.clear();
    }
    return *this;
}

CGroup::~CGroup() {
    if (!path_.empty()) {
        ::rmdir(path_.c_str());
    }
}

const std::string& CGroup::path() const noexcept {
    return path_;
}

bool CGroup::set_memory_limit(std::uint64_t bytes) {
    return write_file(path_ + "/memory.max", std::to_string(bytes));
}

bool CGroup::set_pids_limit(std::uint64_t count) {
    return write_file(path_ + "/pids.max", std::to_string(count));
}

bool CGroup::set_cpu_limit(double cpus) {
    constexpr std::uint64_t period = 100000;
    const auto quota = static_cast<std::uint64_t>(cpus * static_cast<double>(period));
    return write_file(path_ + "/cpu.max", std::to_string(quota) + " " + std::to_string(period));
}

bool CGroup::add_process(pid_t pid) {
    return write_file(path_ + "/cgroup.procs", std::to_string(pid));
}

void CGroup::cleanup() {
    if (!path_.empty()) {
        ::rmdir(path_.c_str());
        path_.clear();
    }
}

} // namespace ctr
