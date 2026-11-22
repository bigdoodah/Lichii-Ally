// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <new>
#include <optional>
#include <utility>

namespace Core {

constexpr std::chrono::seconds ZeroTrimInterval(std::chrono::seconds previous,
                                                std::chrono::milliseconds elapsed,
                                                std::size_t trimmed) {
    using namespace std::chrono_literals;
    const auto yield_interval = trimmed < (std::size_t{64} << 20)
                                    ? (std::min)(previous * 2, 300s) : 5s;
    // Aim for <=1% scan duty, capped at a minute so useful reclamation continues.
    const auto cost_interval = std::chrono::seconds{
        (std::clamp(elapsed.count(), decltype(elapsed.count()){0},
                    decltype(elapsed.count()){600}) + 9) / 10};
    return (std::max)(yield_interval, (std::min)(cost_interval, 60s));
}

template <typename Trim>
std::optional<std::pair<std::size_t, std::size_t>> TryZeroTrim(Trim trim) {
    try {
        return trim();
    } catch (const std::bad_alloc&) {
        // Optional maintenance must not terminate the emulator under memory pressure.
        return std::nullopt;
    }
}

} // namespace Core
