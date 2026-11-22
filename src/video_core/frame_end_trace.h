// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstdlib>
#include <cstring>

#include "common/logging.h"

namespace VideoCommon::FrameEndTrace {

[[nodiscard]] inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NX2_FRAME_END_TRACE");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

class Scope {
public:
    explicit Scope(const char* site_) : site{site_}, active{Enabled()} {
        if (active) {
            started = std::chrono::steady_clock::now();
        }
    }

    ~Scope() {
        Finish();
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    void Finish() {
        if (!active) {
            return;
        }
        active = false;
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (elapsed >= std::chrono::milliseconds{100}) {
            LOG_INFO(Render_Vulkan, "Frame-end slow path: {} took {:.3f} ms", site,
                     std::chrono::duration<double, std::milli>(elapsed).count());
        }
    }

private:
    const char* site;
    bool active;
    std::chrono::steady_clock::time_point started{};
};

} // namespace VideoCommon::FrameEndTrace
