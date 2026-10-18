#include "ctr/container.hpp"

#include "ctr/capabilities.hpp"
#include "ctr/cgroup.hpp"
#include "ctr/logger.hpp"
#include "ctr/namespaces.hpp"
#include "ctr/rootfs.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sched.h>
#include <stdexcept>
#include <string>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ctr {
namespace {

constexpr std::size_t kChildStackSize = 1024 * 1024;

struct ChildContext {
    const Config* config;
};

// Signals the runtime reacts to (used both by the parent and by init).
sigset_t forwarded_signals() {
    sigset_t set;
    ::sigemptyset(&set);
    ::sigaddset(&set, SIGCHLD);
    ::sigaddset(&set, SIGINT);
    ::sigaddset(&set, SIGTERM);
    ::sigaddset(&set, SIGHUP);
    ::sigaddset(&set, SIGQUIT);
    return set;
}

int exit_code_from_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

// Runs as PID 1 inside the container: fork the real command, forward signals to
// it and reap every orphaned grandchild so no zombies pile up.
int run_init(const std::vector<std::string>& command) {
    ::prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);

    const sigset_t set = forwarded_signals();
    ::sigprocmask(SIG_BLOCK, &set, nullptr);

    const pid_t child = ::fork();
    if (child < 0) {
        std::fprintf(stderr, "init: fork: %s\n", std::strerror(errno));
        return 126;
    }
    if (child == 0) {
        ::sigprocmask(SIG_UNBLOCK, &set, nullptr);
        ::setpgid(0, 0);

        std::vector<char*> argv;
        argv.reserve(command.size() + 1);
        for (const auto& part : command) {
            argv.push_back(const_cast<char*>(part.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv.front(), argv.data());
        std::fprintf(stderr, "execvp %s: %s\n", argv.front(), std::strerror(errno));
        _exit(127);
    }

    ::setpgid(child, child);

    int exit_code = 1;
    pid_t tracked = child;
    for (;;) {
        siginfo_t info{};
        if (::sigwaitinfo(&set, &info) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (info.si_signo == SIGCHLD) {
            int status = 0;
            pid_t reaped = 0;
            while ((reaped = ::waitpid(-1, &status, WNOHANG)) > 0) {
                if (reaped == tracked) {
                    exit_code = exit_code_from_status(status);
                    tracked = -1;
                }
            }
            if (tracked == -1) {
                break; // the command is gone; leave the rest to the host init
            }
        } else if (tracked > 0) {
            ::kill(-tracked, info.si_signo); // deliver to the command's process group
        }
    }
    return exit_code;
}

int run_child(const Config& config) {
    try {
        if (::sethostname(config.hostname.c_str(), config.hostname.size()) != 0) {
            throw std::runtime_error(std::string("sethostname: ") + std::strerror(errno));
        }

        setup_rootfs(config.rootfs);

        if (config.drop_capabilities) {
            drop_capabilities();
        }

        return run_init(config.command);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "container: %s\n", ex.what());
        return 126;
    }
}

int child_entry(void* arg) {
    auto* context = static_cast<ChildContext*>(arg);
    _exit(run_child(*context->config));
}

} // namespace

int run_container(const Config& config) {
    std::optional<CGroup> cgroup;
    if (config.use_cgroups) {
        cgroup = CGroup::create("mini-ctr-" + std::to_string(::getpid()));
        if (!cgroup) {
            log(LogLevel::Warn, "cgroups v2 unavailable; running without limits");
        }
    }

    // Block the signals we forward before cloning so none is lost in the race
    // between clone() and the wait loop.
    const sigset_t set = forwarded_signals();
    ::sigprocmask(SIG_BLOCK, &set, nullptr);

    ChildContext context{&config};
    void* stack = std::malloc(kChildStackSize);
    if (stack == nullptr) {
        throw std::runtime_error("out of memory allocating child stack");
    }

    const int flags = namespace_flags(config) | SIGCHLD;
    const pid_t pid =
        ::clone(child_entry, static_cast<char*>(stack) + kChildStackSize, flags, &context);
    if (pid < 0) {
        std::free(stack);
        throw std::runtime_error(std::string("clone: ") + std::strerror(errno));
    }

    if (cgroup) {
        if (!cgroup->add_process(pid)) {
            log(LogLevel::Warn, "failed to add process to cgroup");
        }
        if (config.memory_bytes && !cgroup->set_memory_limit(*config.memory_bytes)) {
            log(LogLevel::Warn, "failed to set memory limit");
        }
        if (config.pids_limit && !cgroup->set_pids_limit(*config.pids_limit)) {
            log(LogLevel::Warn, "failed to set pids limit");
        }
        if (config.cpus && !cgroup->set_cpu_limit(*config.cpus)) {
            log(LogLevel::Warn, "failed to set cpu limit");
        }
    }

    int exit_code = 1;
    for (;;) {
        siginfo_t info{};
        if (::sigwaitinfo(&set, &info) < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (info.si_signo == SIGCHLD) {
            int status = 0;
            pid_t reaped = 0;
            while ((reaped = ::waitpid(-1, &status, WNOHANG)) > 0) {
                if (reaped == pid) {
                    exit_code = exit_code_from_status(status);
                }
            }
            break;
        }
        ::kill(pid, info.si_signo); // forward to the container's init
    }

    std::free(stack);
    if (cgroup) {
        cgroup->cleanup();
    }
    return exit_code;
}

} // namespace ctr
