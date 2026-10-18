#include "ctr/cli.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);

    std::vector<std::string_view> args;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= input.size(); ++i) {
        if (i == input.size() || input[i] == ' ') {
            args.push_back(input.substr(start, i - start));
            start = i + 1;
        }
    }

    if (!args.empty()) {
        const ctr::ParseResult result = ctr::parse_args(args);
        (void)result;
    }
    for (const std::string_view arg : args) {
        (void)ctr::parse_size(arg);
    }
    return 0;
}
