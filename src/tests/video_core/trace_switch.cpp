// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "video_core/renderer_vulkan/trace_switch.h"

TEST_CASE("Ally X Vulkan wait tracing is opt-in and disabled by zero",
          "[video_core][allyx][trace]") {
    using Vulkan::TraceSwitch::IsEnabledValue;
    CHECK_FALSE(IsEnabledValue(nullptr));
    CHECK_FALSE(IsEnabledValue(""));
    CHECK_FALSE(IsEnabledValue("0"));
    CHECK_FALSE(IsEnabledValue("false"));
    CHECK_FALSE(IsEnabledValue("true"));
    CHECK_FALSE(IsEnabledValue("01"));
    CHECK_FALSE(IsEnabledValue("10"));
    CHECK_FALSE(IsEnabledValue("1 "));
    CHECK(IsEnabledValue("1"));
}
