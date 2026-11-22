// SPDX-License-Identifier: GPL-3.0-or-later
#include "common/memory_detect.h"
#include "video_core/texture_cache/eviction_age.h"

constexpr u64 GiB = u64{1} << 30;
using VideoCommon::EvictionAge;
// Recently used images survive routine pressure, but allocation failures and
// the retention boundary must allow emergency eviction immediately.
static_assert(EvictionAge(10, 6 * GiB, 7 * GiB, false) == 150);
static_assert(EvictionAge(25, 6 * GiB, 7 * GiB, false) == 150);
static_assert(EvictionAge(50, 6 * GiB, 7 * GiB, false) == 150);
static_assert(EvictionAge(10, 7 * GiB, 7 * GiB, false) == 10);
static_assert(EvictionAge(10, 8 * GiB, 7 * GiB, false) == 10);
static_assert(EvictionAge(10, 6 * GiB, 7 * GiB, true) == 10);
static_assert(EvictionAge(50, 6 * GiB, 0, false) == 50);
static_assert(!Common::IsLowOnMemory(5 * GiB, 24 * GiB));
static_assert(Common::IsLowOnMemory(3 * GiB, 24 * GiB));
static_assert(!Common::IsLowOnMemory(4 * GiB, 24 * GiB));
static_assert(Common::IsLowOnMemory(GiB, 8 * GiB));
static_assert(!Common::IsLowOnMemory(2 * GiB, 8 * GiB));
int main() { return 0; }
