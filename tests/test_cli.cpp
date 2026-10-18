#include "ctr/cli.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace {

ctr::ParseResult parse(std::vector<std::string_view> args) {
    return ctr::parse_args(args);
}

} // namespace

TEST(ParseSize, PlainBytes) {
    EXPECT_EQ(ctr::parse_size("1048576"), 1048576ULL);
    EXPECT_EQ(ctr::parse_size("0"), 0ULL);
}

TEST(ParseSize, Suffixes) {
    EXPECT_EQ(ctr::parse_size("512b"), 512ULL);
    EXPECT_EQ(ctr::parse_size("1k"), 1024ULL);
    EXPECT_EQ(ctr::parse_size("64M"), 64ULL * 1024 * 1024);
    EXPECT_EQ(ctr::parse_size("2g"), 2ULL * 1024 * 1024 * 1024);
}

TEST(ParseSize, RejectsGarbage) {
    EXPECT_FALSE(ctr::parse_size(""));
    EXPECT_FALSE(ctr::parse_size("abc"));
    EXPECT_FALSE(ctr::parse_size("12x"));
    EXPECT_FALSE(ctr::parse_size("m"));
    EXPECT_FALSE(ctr::parse_size("1.5m"));
    EXPECT_FALSE(ctr::parse_size("-5"));
}

TEST(ParseSize, RejectsOverflow) {
    EXPECT_FALSE(ctr::parse_size("18446744073709551616"));
    EXPECT_FALSE(ctr::parse_size("18446744073709551615g"));
}

TEST(ParseArgs, RunWithCommand) {
    const auto result = parse({"run", "/bin/echo", "hi"});
    ASSERT_EQ(result.status, ctr::ParseStatus::Ok);
    ASSERT_EQ(result.config.command.size(), 2U);
    EXPECT_EQ(result.config.command[0], "/bin/echo");
    EXPECT_EQ(result.config.command[1], "hi");
    EXPECT_EQ(result.config.rootfs, "./rootfs");
}

TEST(ParseArgs, OptionsBeforeCommand) {
    const auto result = parse({"run", "--rootfs", "/tmp/rootfs", "--hostname=box", "--memory",
                               "64m", "--pids=32", "--cpus", "0.5", "/bin/sh"});
    ASSERT_EQ(result.status, ctr::ParseStatus::Ok);
    EXPECT_EQ(result.config.rootfs, "/tmp/rootfs");
    EXPECT_EQ(result.config.hostname, "box");
    EXPECT_EQ(result.config.memory_bytes, 64ULL * 1024 * 1024);
    EXPECT_EQ(result.config.pids_limit, 32ULL);
    ASSERT_TRUE(result.config.cpus.has_value());
    EXPECT_DOUBLE_EQ(*result.config.cpus, 0.5);
}

TEST(ParseArgs, FlagsAfterCommandBelongToCommand) {
    const auto result = parse({"run", "/bin/sh", "-c", "echo --rootfs"});
    ASSERT_EQ(result.status, ctr::ParseStatus::Ok);
    ASSERT_EQ(result.config.command.size(), 3U);
    EXPECT_EQ(result.config.command[2], "echo --rootfs");
}

TEST(ParseArgs, ToggleFlags) {
    const auto result =
        parse({"run", "--keep-caps", "--no-cgroups", "--host-pid", "--net", "/bin/true"});
    ASSERT_EQ(result.status, ctr::ParseStatus::Ok);
    EXPECT_FALSE(result.config.drop_capabilities);
    EXPECT_FALSE(result.config.use_cgroups);
    EXPECT_FALSE(result.config.new_pid_namespace);
    EXPECT_TRUE(result.config.new_network_namespace);
}

TEST(ParseArgs, Help) {
    EXPECT_EQ(parse({}).status, ctr::ParseStatus::Error);
    EXPECT_EQ(parse({"--help"}).status, ctr::ParseStatus::Help);
    EXPECT_EQ(parse({"run", "-h"}).status, ctr::ParseStatus::Help);
}

TEST(ParseArgs, Errors) {
    EXPECT_EQ(parse({"serve"}).status, ctr::ParseStatus::Error);
    EXPECT_EQ(parse({"run"}).status, ctr::ParseStatus::Error);
    EXPECT_EQ(parse({"run", "--bogus", "/bin/true"}).status, ctr::ParseStatus::Error);
    EXPECT_EQ(parse({"run", "--rootfs"}).status, ctr::ParseStatus::Error);
    EXPECT_EQ(parse({"run", "--memory", "huge", "/bin/true"}).status, ctr::ParseStatus::Error);
}
