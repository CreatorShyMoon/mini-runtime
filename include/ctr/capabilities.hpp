#pragma once

namespace ctr {

// Drop every capability from the effective/permitted/inheritable sets and the
// bounding set, then set PR_SET_NO_NEW_PRIVS. Best effort: failures are logged.
void drop_capabilities();

} // namespace ctr
