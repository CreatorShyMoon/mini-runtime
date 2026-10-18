#include "ctr/config.hpp"

#include <gtest/gtest.h>

namespace {

ctr::Config make_valid() {
    ctr::Config config;
    config.rootfs = "/tmp/rootfs";
    config.hostname = "box";
    config.command = {"/bin/sh"};
    return config;
}

} // namespace

TEST(ValidateConfig, AcceptsDefaults) {
    EXPECT_TRUE(ctr::validate_config(make_valid()).empty());
}

TEST(ValidateConfig, RejectsEmptyCommand) {
    auto config = make_valid();
    config.command.clear();
    EXPECT_FALSE(ctr::validate_config(config).empty());
}

TEST(ValidateConfig, RejectsEmptyRootfs) {
    auto config = make_valid();
    config.rootfs.clear();
    EXPECT_FALSE(ctr::validate_config(config).empty());
}

TEST(ValidateConfig, RejectsLongHostname) {
    auto config = make_valid();
    config.hostname = std::string(65, 'a');
    EXPECT_FALSE(ctr::validate_config(config).empty());
}

TEST(ValidateConfig, RejectsNonPositiveLimits) {
    auto memory = make_valid();
    memory.memory_bytes = 0;
    EXPECT_FALSE(ctr::validate_config(memory).empty());

    auto pids = make_valid();
    pids.pids_limit = 0;
    EXPECT_FALSE(ctr::validate_config(pids).empty());

    auto cpus = make_valid();
    cpus.cpus = 0.0;
    EXPECT_FALSE(ctr::validate_config(cpus).empty());
}
