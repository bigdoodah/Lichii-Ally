// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdlib>

#include "common/common_types.h"
#include "common/logging.h"
#include "video_core/renderer_vulkan/trace_switch.h"

namespace Vulkan::PipelineWaitTrace {

using Clock = std::chrono::steady_clock;

inline bool Enabled() {
    static const bool enabled = TraceSwitch::Enabled("EDEN_NX2_LIST_WAIT_TRACE");
    return enabled;
}

inline bool ShouldReport(Clock::duration held, Clock::time_point end) {
    using namespace std::chrono_literals;
    if (held < 100ms || !Enabled()) {
        return false;
    }
    static thread_local Clock::time_point next_report{};
    if (held < 1s && end < next_report) {
        return false;
    }
    next_report = end + 500ms;
    return true;
}

inline void Report(const char* role, u64 key, u64 tick, bool ran_build, Clock::duration held) {
    LOG_WARNING(Render_Vulkan, "Vulkan worker pipeline wait: {} {:#018x}, command tick {}, "
                               "{:.3f} ms, {}",
                role, key, tick, std::chrono::duration<double, std::milli>(held).count(),
                ran_build ? "built on Vulkan worker" : "waited for compiler");
}

} // namespace Vulkan::PipelineWaitTrace
