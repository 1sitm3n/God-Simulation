// This file exists so the godsim_lib static library has at least one
// translation unit. Core systems are header-only for now; implementation
// files will be added as systems grow more complex.

namespace godsim {
    // Library version
    const char* version() { return "0.1.0"; }
}
