#pragma once

#include "ctr/config.hpp"

namespace ctr {

// Clone into the requested namespaces, set up the rootfs/cgroups/capabilities
// and exec the configured command. Returns the container's exit code
// (128 + signal when killed by a signal). Throws std::runtime_error only on
// errors that occur in the parent before the child is reaped.
int run_container(const Config& config);

} // namespace ctr
