// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include "video_core/renderer_vulkan/pipeline_retirement.h"
#include "video_core/pipeline_cache_policy.h"

using namespace std::chrono_literals;
void Check(bool value) { if (!value) std::abort(); }

struct Queue {
    std::deque<int> entries;
    std::chrono::steady_clock::time_point time{};
    std::chrono::microseconds cost{};
    bool empty() const { return entries.empty(); }
    int front() const { return entries.front(); }
    void pop_front() { entries.pop_front(); time += cost; }
};

int main() {
    Queue q;
    const auto run = [&](bool pressure) {
        return Vulkan::RetireReadyPipelines(q, [](int fence) { return fence == 0; },
                                           [&] { return q.time; }, pressure);
    };
    Check(run(false) == 0);
    q.entries.assign(2400, 0);
    Check(run(false) == 32 && q.entries.size() == 2368);
    Check(run(true) == 128);
    q.entries.front() = 1;
    Check(run(false) == 0); // An unfinished GPU fence must never release its object.
    q.entries.front() = 0;
    q.cost = 600us;
    Check(run(false) == 2); // Stop after crossing the soft deadline.
    Check(run(true) == 4);
    q.cost = 5ms;
    Check(run(false) == 1); // One slow driver call cannot stall the queue indefinitely.
    q.cost = 0us;
    while (!q.empty()) Check(run(false) > 0);
    Check(VideoCommon::GraphicsPipelinesToEvict(2200, 1000, 2237) == 0);
    Check(VideoCommon::GraphicsPipelinesToEvict(2400, 1000, 2237) == 163);
    std::cout << "Pipeline retirement: fence safety, batch limits, deadlines, progress, eviction bounds passed\n";
}
