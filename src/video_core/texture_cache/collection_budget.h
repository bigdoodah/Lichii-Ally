// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>

namespace VideoCommon {

// A download or driver call cannot be interrupted. Always allow one eligible image to
// make progress, but share the deadline across every pass in this collection.
class CollectionBudget {
public:
    using Clock = std::chrono::steady_clock;

    CollectionBudget(Clock::time_point start, bool emergency, bool pressure)
        : deadline{start + std::chrono::milliseconds{pressure ? 4 : 2}}, unlimited{emergency} {}

    bool Exhausted(Clock::time_point now) const {
        return !unlimited && started && now >= deadline;
    }

    void StartImage() {
        started = true;
    }

private:
    Clock::time_point deadline;
    bool unlimited;
    bool started{};
};

} // namespace VideoCommon
