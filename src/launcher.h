#pragma once

#include "desktopentry.h"

namespace kestrel {

/// Starts the application described by `entry` detached from the launcher.
bool launch(const DesktopEntry &entry);

/// Terminates the process behind `entry` (best effort, uses `pkill -f`).
bool terminate(const DesktopEntry &entry);

} // namespace kestrel
