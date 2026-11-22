// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/common_types.h"

namespace Tegra {

// The cache retains ownership of staging. A retry must look the page up again.
struct PendingListCopy {
    u64 tick{};
    u64 writer{};
    DAddr device_addr{};
    u64 writes{};
    bool driver_returned_at_start{};
};

} // namespace Tegra
