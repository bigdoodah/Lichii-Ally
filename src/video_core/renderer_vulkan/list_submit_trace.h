// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Vulkan {

// The Vulkan worker records successful queue calls. The GPU front looks up an exact tick
// only when reporting a long wait. Older ticks may have been overwritten.
class ListSubmitTrace {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t Capacity = 8192;

    struct Stages {
        Clock::duration before;
        Clock::duration inside;
        Clock::duration after;
    };

    void Record(std::uint64_t tick, Clock::time_point entered, Clock::time_point returned) {
        std::scoped_lock lock{mutex};
        records[tick % Capacity] = {tick, entered, returned};
    }

    [[nodiscard]] std::optional<Stages> Measure(std::uint64_t tick, Clock::time_point start,
                                                Clock::time_point observed) const {
        std::scoped_lock lock{mutex};
        const auto& record = records[tick % Capacity];
        if (tick == 0 || record.tick != tick || observed < start) {
            return std::nullopt;
        }
        const auto enter = std::clamp(record.entered, start, observed);
        const auto leave = std::clamp(record.returned, start, observed);
        return Stages{enter - start, leave - enter, observed - leave};
    }

private:
    struct RecordTimes {
        std::uint64_t tick{};
        Clock::time_point entered{};
        Clock::time_point returned{};
    };
    mutable std::mutex mutex;
    std::array<RecordTimes, Capacity> records{};
};

} // namespace Vulkan
