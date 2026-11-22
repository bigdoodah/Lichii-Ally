// SPDX-License-Identifier: GPL-3.0-or-later
#include <limits>
#include <catch2/catch_test_macros.hpp>
#include "video_core/pipeline_binary_policy.h"

using namespace Vulkan::PipelineBinaryCache;

TEST_CASE("AMD binaries keep a preparation session beyond the old 384 MiB limit",
          "[video_core][pipeline_binary]") {
    CHECK(RetainedBytes(600ULL << 20) >= (600ULL << 20));
    CHECK(RetainedBytes(1536ULL << 20) == (1536ULL << 20));
    CHECK(RetainedBytes(3ULL << 30) == (3ULL << 30));
    CHECK(RetainedBytes(5ULL << 30) == SessionCapBytes);
    CHECK(RetainedBytes(0) < MaxFileBytes);
}

TEST_CASE("Pipeline binary append ceiling cannot overflow or exceed four GiB",
          "[video_core][pipeline_binary]") {
    CHECK(FitsRecord(SessionCapBytes - 64, 64));
    CHECK_FALSE(FitsRecord(SessionCapBytes - 64, 65));
    CHECK_FALSE(FitsRecord(SessionCapBytes, 1));
    CHECK_FALSE(FitsRecord(SessionCapBytes + 1, 0));
    CHECK_FALSE(FitsRecord(1, std::numeric_limits<u64>::max()));
}
