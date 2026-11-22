// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include "common/common_types.h"

namespace Vulkan::PipelineBinaryCache {
inline constexpr u64 MaxFileBytes = 1ULL << 30;
inline constexpr u64 SessionCapBytes = 4ULL << 30;

// A preparation session must not lose the binaries it just finished building (#1407).
constexpr u64 RetainedBytes(u64 session_bytes) {
    return std::max(MaxFileBytes / 8 * 7, std::min(session_bytes, SessionCapBytes));
}

constexpr bool FitsRecord(u64 file_bytes, u64 record_bytes) {
    return file_bytes <= SessionCapBytes && record_bytes <= SessionCapBytes - file_bytes;
}
} // namespace Vulkan::PipelineBinaryCache
