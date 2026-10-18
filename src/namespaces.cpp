#include "ctr/namespaces.hpp"

#include <sched.h>

namespace ctr {

int namespace_flags(const Config& config) {
    int flags = CLONE_NEWNS | CLONE_NEWUTS;
    if (config.new_pid_namespace) {
        flags |= CLONE_NEWPID;
    }
    if (config.new_ipc_namespace) {
        flags |= CLONE_NEWIPC;
    }
    if (config.new_network_namespace) {
        flags |= CLONE_NEWNET;
    }
    return flags;
}

} // namespace ctr
