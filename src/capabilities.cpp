#include "ctr/capabilities.hpp"

#include "ctr/logger.hpp"

#include <fstream>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace ctr {
namespace {

int last_capability() {
    std::ifstream file("/proc/sys/kernel/cap_last_cap");
    int value = 40;
    if (file) {
        file >> value;
    }
    return value;
}

} // namespace

void drop_capabilities() {
    __user_cap_header_struct header{};
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;

    __user_cap_data_struct data[2]{};
    if (::syscall(SYS_capset, &header, data) != 0) {
        log(LogLevel::Warn, "capset failed; capabilities not cleared");
    }

    const int last = last_capability();
    for (int cap = 0; cap <= last; ++cap) {
        ::prctl(PR_CAPBSET_DROP, cap, 0, 0, 0);
    }
    ::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
}

} // namespace ctr
