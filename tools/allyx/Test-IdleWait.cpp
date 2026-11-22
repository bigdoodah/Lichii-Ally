// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include "common/thread.h"
using namespace std::chrono_literals;
static_assert(Common::NextPollWindow(100us, 3us, 100us) == 100us);
static_assert(Common::NextPollWindow(25us, 40us, 100us) == 50us);
static_assert(Common::NextPollWindow(75us, 90us, 100us) == 100us);
static_assert(Common::NextPollWindow(100us, 101us, 100us) == 50us);
static_assert(Common::NextPollWindow(12500ns, 2ms, 100us) == 12500ns);
int main() {
    std::atomic<bool> flag{false};
    auto start = std::chrono::steady_clock::now();
    if (Common::WaitForFlag(flag, start + 2ms) ||
        std::chrono::steady_clock::now() - start < 2ms) return 1;
    if (Common::WaitForFlag(flag, start)) return 2;
    for (int i = 0; i < 20; ++i) {
        flag = false;
        std::jthread setter([&] {
            std::this_thread::sleep_for(1ms);
            flag.store(true, std::memory_order_release);
        });
        if (!Common::WaitForFlag(flag, std::chrono::steady_clock::now() + 1s)) return 3;
    }
    if (!Common::WaitForFlag(flag, std::chrono::steady_clock::now())) return 4;
    std::puts("Passed idle timeout, wake and adaptive-window checks");
}
