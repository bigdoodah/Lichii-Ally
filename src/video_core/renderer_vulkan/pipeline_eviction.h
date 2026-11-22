// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstddef>

namespace Vulkan {

// Selection scans one hash partition per presented frame. We do not retain flat-map
// iterators across frames: insertions may rehash and invalidate them.
inline constexpr std::size_t GraphicsEvictionShardCount = 128;
inline constexpr std::size_t GraphicsEvictionQueueBatch = 64;
static_assert((GraphicsEvictionShardCount & (GraphicsEvictionShardCount - 1)) == 0);

[[nodiscard]] constexpr bool InGraphicsEvictionShard(std::size_t hash,
                                                      std::size_t shard) noexcept {
    return shard < GraphicsEvictionShardCount &&
           (hash & (GraphicsEvictionShardCount - 1)) == shard;
}

[[nodiscard]] constexpr std::size_t GraphicsEvictionQueueEnd(std::size_t queued,
                                                              std::size_t selected) noexcept {
    return queued >= selected ? selected :
           queued + ((selected - queued < GraphicsEvictionQueueBatch) ?
                         (selected - queued) : GraphicsEvictionQueueBatch);
}
// Revalidation and metadata work can fault or allocate. The deadline is soft; even skipped
// candidates count toward the hard limit so a queue of newly reused pipelines stays bounded.
template <typename Queue, typename Evict, typename Now>
std::size_t DrainPipelineEvictions(Queue& queue, Evict evict, Now now, bool memory_pressure) {
    if (queue.empty()) {
        return 0;
    }
    const std::size_t limit = memory_pressure ? 128 : 32;
    const auto deadline = now() + std::chrono::milliseconds{memory_pressure ? 2 : 1};
    std::size_t examined = 0;
    std::size_t removed = 0;
    while (!queue.empty() && examined < limit) {
        if (examined != 0 && now() >= deadline) {
            break;
        }
        removed += evict(queue.front()) ? 1 : 0;
        queue.pop_front();
        ++examined;
    }
    return removed;
}

} // namespace Vulkan
