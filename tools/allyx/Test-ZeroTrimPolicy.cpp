// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include "core/zero_trim_policy.h"
using namespace std::chrono_literals;
constexpr std::size_t MiB = 1024 * 1024;
static_assert(Core::ZeroTrimInterval(5s, 1470ms, 199 * MiB) == 60s);
static_assert(Core::ZeroTrimInterval(5s, 1460ms, 16 * MiB) == 60s);
static_assert(Core::ZeroTrimInterval(5s, 40ms, 100 * MiB) == 5s);
static_assert(Core::ZeroTrimInterval(60s, 40ms, 100 * MiB) == 5s);
static_assert(Core::ZeroTrimInterval(160s, 40ms, MiB) == 300s);
static_assert(Core::ZeroTrimInterval(5s, 251ms, 100 * MiB) == 26s);
int main() {
    const auto failure = Core::TryZeroTrim([]() -> std::pair<std::size_t, std::size_t> {
        throw std::bad_alloc{};
    });
    if (failure) std::abort();
    const auto success = Core::TryZeroTrim([] { return std::pair<std::size_t, std::size_t>{8, 4}; });
    if (!success || success->first != 8 || success->second != 4) std::abort();
    bool propagated = false;
    try {
        Core::TryZeroTrim([]() -> std::pair<std::size_t, std::size_t> {
            throw std::runtime_error{"unrelated failure"};
        });
    } catch (const std::runtime_error&) { propagated = true; }
    if (!propagated) std::abort();
    std::cout << "Zero-trim cooldown, recovery, bad_alloc and exception scope passed\n";
}
