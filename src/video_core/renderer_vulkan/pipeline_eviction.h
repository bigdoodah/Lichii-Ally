// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstddef>

namespace Vulkan {

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
