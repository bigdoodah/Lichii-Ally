// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include "common/common_types.h"

namespace VideoCommon {

constexpr u64 CostlyRetentionFrames = 1800;
constexpr u64 CostlyDeviceReserve = u64{512} << 20;
constexpr u64 CostlyCommitReserve = u64{2} << 30;
constexpr u64 CostlyPhysicalReserve = u64{1} << 30;

// Unknown host-memory values disable protection. Usage includes allocations outside the cache.
[[nodiscard]] constexpr bool CanRetainCostlyTexture(u64 usage, u64 budget, u64 slack,
                                                   u64 commit_room, u64 physical_room,
                                                   bool allocation_pressure) {
    if (allocation_pressure || commit_room < CostlyCommitReserve ||
        physical_room < CostlyPhysicalReserve || budget <= CostlyDeviceReserve) {
        return false;
    }
    const u64 limit = budget - CostlyDeviceReserve;
    return slack < limit && usage < limit - slack;
}

} // namespace VideoCommon
