// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>

#include "video_core/renderer_vulkan/pipeline_eviction.h"
#include "video_core/renderer_vulkan/pipeline_transition_cache.h"

using namespace std::chrono_literals;

void Check(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    std::deque<int> candidates;
    std::chrono::steady_clock::time_point time{};
    std::chrono::microseconds cost{};
    bool eligible = true;
    const auto drain = [&](bool pressure) {
        return Vulkan::DrainPipelineEvictions(
            candidates, [&](int) { time += cost; return eligible; }, [&] { return time; }, pressure);
    };
    Check(drain(false) == 0);
    candidates.assign(1802, 0);
    Check(drain(false) == 32 && candidates.size() == 1770);
    Check(drain(true) == 128 && candidates.size() == 1642);
    eligible = false;
    Check(drain(false) == 0 && candidates.size() == 1610);
    cost = 600us;
    Check(drain(false) == 0 && candidates.size() == 1608);
    eligible = true;
    Check(drain(false) == 2);
    Check(drain(true) == 4);
    cost = 5ms;
    Check(drain(false) == 1);
    cost = 0us;
    candidates.assign(1802, 0);
    std::size_t frames = 0;
    std::size_t removed = 0;
    while (!candidates.empty()) {
        removed += drain(false);
        ++frames;
    }
    Check(removed == 1802 && frames == 57);

    Vulkan::PipelineTransitionCache<int, int> transitions;
    auto old_pipeline = std::make_unique<int>(1);
    transitions.Add(1, old_pipeline.get(), 0);
    Check(transitions.Find(1, 0) == old_pipeline.get());
    old_pipeline.reset();
    Check(transitions.Find(1, 1) == nullptr); // An evicted object's pointer must not escape.

    int retained = 2;
    int replacement = 3;
    transitions.Add(2, &retained, 1);
    // Adding without an intervening lookup must also drop the previous generation.
    transitions.Add(3, &replacement, 2);
    Check(transitions.Find(2, 2) == nullptr);
    Check(transitions.Find(3, 2) == &replacement);
    Check(transitions.Find(3, 4) == nullptr); // Several eviction batches while unused.
    transitions.Add(3, &replacement, 4);
    transitions.Clear(); // Failed-precompiled cleanup keeps its explicit invalidation.
    Check(transitions.Find(3, 4) == nullptr);
    transitions.Add(3, &retained, 4); // The same key can name a rebuilt object.
    Check(transitions.Find(3, 4) == &retained);
    std::cout << "Pipeline eviction: count/time limits, skipped candidates, complete draining, "
                 "and stale transition invalidation passed\n";
}
