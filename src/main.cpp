#include "ctr/cli.hpp"
#include "ctr/container.hpp"

#include <cstdio>
#include <exception>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const ctr::ParseResult result = ctr::parse_args(args);
    switch (result.status) {
    case ctr::ParseStatus::Help:
        std::fputs(ctr::usage().c_str(), stdout);
        return 0;
    case ctr::ParseStatus::Error:
        std::fprintf(stderr, "error: %s\n\n", result.error.c_str());
        std::fputs(ctr::usage().c_str(), stderr);
        return 2;
    case ctr::ParseStatus::Ok:
        break;
    }

    try {
        return ctr::run_container(result.config);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    }
}
