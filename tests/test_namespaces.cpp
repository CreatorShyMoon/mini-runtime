#include "ctr/config.hpp"
#include "ctr/namespaces.hpp"

#include <gtest/gtest.h>
#include <sched.h>

TEST(NamespaceFlags, AlwaysIncludesMountAndUts) {
    ctr::Config config;
    const int flags = ctr::namespace_flags(config);
    EXPECT_NE(flags & CLONE_NEWNS, 0);
    EXPECT_NE(flags & CLONE_NEWUTS, 0);
}

TEST(NamespaceFlags, DefaultsIncludePidAndIpc) {
    ctr::Config config;
    const int flags = ctr::namespace_flags(config);
    EXPECT_NE(flags & CLONE_NEWPID, 0);
    EXPECT_NE(flags & CLONE_NEWIPC, 0);
    EXPECT_EQ(flags & CLONE_NEWNET, 0);
}

TEST(NamespaceFlags, HonorToggles) {
    ctr::Config config;
    config.new_pid_namespace = false;
    config.new_network_namespace = true;
    const int flags = ctr::namespace_flags(config);
    EXPECT_EQ(flags & CLONE_NEWPID, 0);
    EXPECT_NE(flags & CLONE_NEWNET, 0);
}
