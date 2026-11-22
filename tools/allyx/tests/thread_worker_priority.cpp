// SPDX-License-Identifier: GPL-3.0-or-later
#include <atomic>
#include <cassert>
#include <optional>
#include "common/thread_worker.h"

// Isolate the pool's policy from platform scheduling and test each worker's task context.
namespace {
thread_local std::optional<Common::ThreadPriority> applied;
thread_local Common::ThreadPlacement placed;
}
namespace Common {
void SetCurrentThreadName(const char*) {}
void SetCurrentThreadPriority(ThreadPriority priority) { applied = priority; }
void SetCurrentThreadToAllCores() { placed = ThreadPlacement::Default; }
void SetCurrentThreadToBackgroundWork() { placed = ThreadPlacement::Background; }
void SetCurrentThreadToEfficiencyCores() { placed = ThreadPlacement::Efficiency; }
}

int main() {
    using namespace Common;
    const auto check = [](ThreadPlacement placement, std::optional<ThreadPriority> override_value,
                          std::optional<ThreadPriority> expected) {
        std::atomic<int> done{};
        ThreadWorker pool{3, "policy-test", {}, placement, override_value};
        for (int i = 0; i < 100; ++i) {
            pool.QueueWork([&] {
                assert(applied == expected);
                assert(placed == placement);
                ++done;
            });
        }
        pool.WaitForRequests();
        assert(done == 100);
    };
    check(ThreadPlacement::Default, {}, {});
    check(ThreadPlacement::Background, {}, ThreadPriority::Low);
    check(ThreadPlacement::Efficiency, {}, ThreadPriority::Low);
    check(ThreadPlacement::Background, ThreadPriority::Normal, ThreadPriority::Normal);
}
