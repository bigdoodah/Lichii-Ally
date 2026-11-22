// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>

namespace Vulkan {

// Used only by the GPU front thread. All pending channels share this refresh budget.
class ListTickPoll {
public:
    bool ShouldRefresh(std::chrono::steady_clock::time_point now) noexcept {
        if (now < next_refresh) {
            return false;
        }
        next_refresh = now + std::chrono::microseconds{20};
        return true;
    }

private:
    std::chrono::steady_clock::time_point next_refresh =
        std::chrono::steady_clock::time_point::min();
};

} // namespace Vulkan
