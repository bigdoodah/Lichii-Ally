// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <deque>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "video_core/texture_cache/transcode_reaper.h"
#include "video_core/renderer_vulkan/vk_astc_transcode.h"

namespace {

struct CompletionScheduler {
    u64 known{};
    u64 completed{};
    unsigned refreshes{};

    bool IsFree(u64 tick) const {
        return known >= tick;
    }
    CompletionScheduler& GetMasterSemaphore() {
        return *this;
    }
    void Refresh() {
        ++refreshes;
        known = completed;
    }
};

struct DecodeImage {
    u64 tick{};
    u64 allocated_bytes{4};
    u32 width{1};
    u32 height{1};
    u32 levels{1};
    u32 layers{1};
    u32 unused_frames{20};
};

} // namespace

TEST_CASE("ASTC reaping refreshes past an already-complete prefix without waiting",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{.known = 10, .completed = 11};
    std::deque<DecodeImage> pending{{.tick = 10}, {.tick = 11}, {.tick = 12}};
    std::vector<DecodeImage> idle;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 64);
    REQUIRE(idle.size() == 2);
    CHECK(idle[0].tick == 10);
    CHECK(idle[1].tick == 11);
    CHECK(idle[1].unused_frames == 0);
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().tick == 12);
    CHECK(scheduler.refreshes == 1);
}

TEST_CASE("ASTC reaping does not query completion when cached progress is sufficient",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{.known = 12, .completed = 12};
    std::deque<DecodeImage> pending{{.tick = 10}, {.tick = 11}};
    std::vector<DecodeImage> idle;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 64);
    CHECK(pending.empty());
    CHECK(idle.size() == 2);
    CHECK(scheduler.refreshes == 0);
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 64);
    CHECK(scheduler.refreshes == 0);
}

TEST_CASE("ASTC reaping preserves unfinished images and enforces the idle budget",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{.known = 0, .completed = 2};
    std::deque<DecodeImage> pending{{.tick = 1}, {.tick = 2}, {.tick = 3}};
    std::vector<DecodeImage> idle{{.tick = 0}};
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 8);
    REQUIRE(idle.size() == 2);
    CHECK(idle[1].tick == 1);
    CHECK(idle[1].unused_frames == 0);
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().tick == 3);
    CHECK(scheduler.refreshes == 1);
}

TEST_CASE("ASTC reaping returns when refreshing reports no progress",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{};
    std::deque<DecodeImage> pending{{.tick = 1}};
    std::vector<DecodeImage> idle;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 64);
    CHECK(idle.empty());
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().tick == 1);
    CHECK(scheduler.refreshes == 1);
}

TEST_CASE("Ally terrain decode scratch can be reused during the same frame",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{.known = 0, .completed = 10};
    std::deque<DecodeImage> pending{
        {.tick = 10, .allocated_bytes = 240ULL << 20, .width = 1365, .height = 1365, .levels = 11, .layers = 23},
        {.tick = 11, .allocated_bytes = 240ULL << 20, .width = 1365, .height = 1365, .levels = 11, .layers = 23}};
    std::vector<DecodeImage> idle;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle,
                                       Vulkan::AstcTranscode::MaxDecodeBytes);
    REQUIRE(idle.size() == 1);
    CHECK(idle.front().tick == 10);
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().tick == 11);
    // The first scratch image is still idle when the next one completes.
    scheduler.completed = 11;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle,
                                       Vulkan::AstcTranscode::MaxDecodeBytes);
    CHECK(pending.empty());
    CHECK(idle.size() == 1);
}

TEST_CASE("ASTC idle budget includes driver padding and rejects unknown sizes",
          "[video_core][transcode_reaper]") {
    CompletionScheduler scheduler{.known = 4};
    std::deque<DecodeImage> pending{
        {.tick = 1, .allocated_bytes = 9},
        {.tick = 2, .allocated_bytes = 0},
        {.tick = 3, .allocated_bytes = 8},
        {.tick = 5, .allocated_bytes = 100}};
    std::vector<DecodeImage> idle;
    VideoCommon::ReapFinishedTranscodes(scheduler, pending, idle, 8);
    REQUIRE(idle.size() == 1);
    CHECK(idle.front().tick == 3);
    REQUIRE(pending.size() == 1);
    CHECK(pending.front().tick == 5);
}
