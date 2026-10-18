#pragma once

#include "ctr/config.hpp"

namespace ctr {

// clone(2) flags implied by the requested namespaces (no signal bits set).
int namespace_flags(const Config& config);

} // namespace ctr
