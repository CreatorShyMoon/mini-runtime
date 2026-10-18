#include "ctr/rootfs.hpp"

#include "ctr/logger.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace ctr {
namespace {

std::runtime_error errno_error(const char* what) {
    return std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void make_node(const char* path, mode_t mode, unsigned major, unsigned minor) {
    if (::mknod(path, mode | S_IFCHR, ::makedev(major, minor)) != 0 && errno != EEXIST) {
        log(LogLevel::Warn, std::string("mknod ") + path + " failed");
    }
}

void make_symlink(const char* target, const char* path) {
    if (::symlink(target, path) != 0 && errno != EEXIST) {
        log(LogLevel::Warn, std::string("symlink ") + path + " failed");
    }
}

// Fallback when devtmpfs is unavailable: a tmpfs /dev with the essential nodes.
void create_device_nodes() {
    make_node("/dev/null", 0666, 1, 3);
    make_node("/dev/zero", 0666, 1, 5);
    make_node("/dev/full", 0666, 1, 7);
    make_node("/dev/random", 0666, 1, 8);
    make_node("/dev/urandom", 0666, 1, 9);
    make_node("/dev/tty", 0666, 5, 0);

    make_symlink("/proc/self/fd", "/dev/fd");
    make_symlink("/proc/self/fd/0", "/dev/stdin");
    make_symlink("/proc/self/fd/1", "/dev/stdout");
    make_symlink("/proc/self/fd/2", "/dev/stderr");
    make_symlink("pts/ptmx", "/dev/ptmx");
}

void mount_virtual_filesystems() {
    if (::mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, nullptr) != 0) {
        log(LogLevel::Debug, "devtmpfs unavailable; falling back to tmpfs /dev");
        if (::mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=0755") != 0) {
            log(LogLevel::Warn, "mount(/dev) failed; /dev will be empty");
        } else {
            create_device_nodes();
        }
    }

    ::mkdir("/dev/pts", 0755);
    if (::mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC,
                "newinstance,ptmxmode=0666,mode=0620") != 0) {
        log(LogLevel::Warn, "mount(/dev/pts) failed");
    }

    ::mkdir("/dev/shm", 01777);
    if (::mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") != 0) {
        log(LogLevel::Warn, "mount(/dev/shm) failed");
    }

    ::mkdir("/sys", 0555);
    if (::mount("sysfs", "/sys", "sysfs", MS_RDONLY | MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr) !=
        0) {
        log(LogLevel::Warn, "mount(/sys) failed");
    }
}

} // namespace

void setup_rootfs(const std::string& rootfs) {
    // 1. Stop the container's mounts from propagating back to the host.
    if (::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        throw errno_error("mount(/, MS_PRIVATE)");
    }

    // 2. pivot_root requires a mount point, so bind-mount rootfs onto itself.
    if (::mount(rootfs.c_str(), rootfs.c_str(), nullptr, MS_BIND | MS_REC, nullptr) != 0) {
        throw errno_error("mount(rootfs, MS_BIND)");
    }

    const std::string oldroot = rootfs + "/.oldroot";
    if (::mkdir(oldroot.c_str(), 0700) != 0 && errno != EEXIST) {
        throw errno_error("mkdir(.oldroot)");
    }

    if (::syscall(SYS_pivot_root, rootfs.c_str(), oldroot.c_str()) == 0) {
        if (::chdir("/") != 0) {
            throw errno_error("chdir(/)");
        }
        if (::umount2("/.oldroot", MNT_DETACH) != 0) {
            log(LogLevel::Warn, "umount2(/.oldroot) failed; continuing");
        }
        ::rmdir("/.oldroot");
    } else {
        // pivot_root is not always permitted; chroot gets us close enough.
        log(LogLevel::Debug, "pivot_root failed; falling back to chroot");
        if (::chroot(rootfs.c_str()) != 0) {
            throw errno_error("chroot");
        }
        if (::chdir("/") != 0) {
            throw errno_error("chdir(/)");
        }
    }

    // 3. A fresh /proc for the new PID namespace.
    if (::mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr) != 0) {
        log(LogLevel::Warn, "mount(/proc) failed; /proc will be empty");
    }

    // 4. /dev, /dev/pts, /dev/shm and a read-only /sys.
    mount_virtual_filesystems();
}

} // namespace ctr
