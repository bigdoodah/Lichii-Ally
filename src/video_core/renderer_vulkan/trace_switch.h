// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdlib>

namespace Vulkan::TraceSwitch {

// Performance24 diagnostic switches are opt-in: only the exact value "1" enables one.
// In particular, an inherited =0 must not allocate tracing buffers or log waits.
[[nodiscard]] constexpr bool IsEnabledValue(const char* value) noexcept {
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

[[nodiscard]] inline bool Enabled(const char* name) noexcept {
    return IsEnabledValue(std::getenv(name));
}

} // namespace Vulkan::TraceSwitch
