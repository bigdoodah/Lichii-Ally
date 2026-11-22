#include <cassert>
#include <limits>
#include "video_core/texture_cache/costly_retention.h"
#include "video_core/texture_cache/eviction_age.h"

int main() {
    using namespace VideoCommon;
    constexpr u64 MiB = u64{1} << 20;
    constexpr u64 GiB = u64{1} << 30;
    const auto allow = [](u64 usage, u64 budget, u64 slack, u64 commit, u64 physical,
                          bool pressure = false) {
        return CanRetainCostlyTexture(usage, budget, slack, commit, physical, pressure);
    };
    assert(allow(3 * GiB, 4 * GiB, 128 * MiB, 3 * GiB, 2 * GiB));
    assert(!allow(3 * GiB, 4 * GiB, 512 * MiB, 3 * GiB, 2 * GiB));
    assert(!allow(3 * GiB, 4 * GiB, 128 * MiB, GiB, 2 * GiB));
    assert(!allow(3 * GiB, 4 * GiB, 128 * MiB, 3 * GiB, 512 * MiB));
    assert(!allow(3 * GiB, 4 * GiB, 128 * MiB, 3 * GiB, 2 * GiB, true));
    assert(!allow(0, 0, 0, 3 * GiB, 2 * GiB));
    assert(!allow(0, 512 * MiB, 0, 3 * GiB, 2 * GiB));
    assert(!allow(0, 4 * GiB, 4 * GiB, 3 * GiB, 2 * GiB));
    assert(!allow(0, 4 * GiB, 0, 0, 2 * GiB));
    assert(!allow(0, 4 * GiB, 0, 3 * GiB, 0));
    const u64 maximum = (std::numeric_limits<u64>::max)();
    assert(!allow(maximum, maximum, maximum, maximum, maximum));
    // A pass starts under pressure, then prior deletions restore its retention headroom.
    assert(EvictionAge(10, 4 * GiB, 4 * GiB, false) == 10);
    assert(EvictionAge(10, 3 * GiB, 4 * GiB, false) == 150);
    assert(EvictionAge(10, 3 * GiB, 4 * GiB, true) == 10);
}
