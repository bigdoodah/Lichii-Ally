// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstddef>

namespace Vulkan {

// Driver destruction cannot be interrupted. Bound both calls and elapsed time between calls.
template <typename Queue, typename Ready, typename Now>
std::size_t RetireReadyPipelines(Queue& queue, Ready ready, Now now, bool memory_pressure) {
    if (queue.empty()) {
        return 0;
    }
    const std::size_t limit = memory_pressure ? 128 : 32;
    const auto deadline = now() + std::chrono::milliseconds{memory_pressure ? 2 : 1};
    std::size_t retired = 0;
    while (!queue.empty() && retired < limit && ready(queue.front())) {
        if (retired != 0 && now() >= deadline) {
            break;
        }
        queue.pop_front();
        ++retired;
    }
    return retired;
}

} // namespace Vulkan
