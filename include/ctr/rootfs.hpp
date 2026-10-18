#pragma once

#include <string>

namespace ctr {

// Turn `rootfs` into the container's root: make mount propagation private,
// pivot_root into it (falling back to chroot) and mount a fresh /proc.
// Throws std::runtime_error on failure. Runs inside the new namespaces.
void setup_rootfs(const std::string& rootfs);

} // namespace ctr
