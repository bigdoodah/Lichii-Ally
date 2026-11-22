"""Compile the command-list copy reader against a controllable Vulkan timeline.

Run with the configured Windows LLVM/MSVC environment:
    python Test-ListCopyWait.py OUTPUT_DIRECTORY
"""
from pathlib import Path
import subprocess
import sys


root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/video_core/buffer_cache/buffer_cache.h").read_text()
start = source.index("auto BufferCache<P>::Nx2ReadListCopy(")
end = source.index("\ntemplate <class P>", start)
reader = source[start:end].replace("BufferCache<P>::", "", 1)
base = (root / "src/video_core/buffer_cache/buffer_cache_base.h").read_text()
start = base.index("enum class Nx2ListRead")
read_result = base[start:base.index(";", start) + 1]

fixture = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>
#include "video_core/pending_list_copy.h"
#include "video_core/renderer_vulkan/list_tick_poll.h"

namespace Tegra { constexpr int Nx2WrittenPageBits = 12; }
namespace Tegra::Nx2ListVerify { constexpr bool t_reference = false; }
namespace VideoCore::Nx2Debug { int Experiment(int) { return 0; } }

struct Timeline {
    u64 submitted = 12;
    u64 done = 4;
    unsigned waits{};
    std::function<void()> on_wait;
    std::function<void()> on_probe;
    bool IsFree(u64 tick) const { return tick <= done; }
    u64 CurrentTick() const { return submitted + 1; }
    bool IsListTickDone(u64 tick) {
        if (on_probe) on_probe();
        return IsFree(tick);
    }
    void WaitSubmittedTick(u64 tick, u64 writer) {
        assert(writer == 51);
        ++waits;
        if (on_wait) on_wait();
        done = tick;
    }
};

struct Nx2ListPage {
    bool copy = true, draw_written = false, flushed = false;
    u64 writes = 5, copy_writes = 5, synced_writes = 0;
    u64 copy_begin = 0, copy_end = 0x1000, copy_tick = 12, copy_writer = 51;
    u8 value = 0x42;
};

struct Cache {
    static constexpr bool IS_OPENGL = false;
    Timeline runtime;
    std::mutex nx2_list_mutex;
    std::unordered_map<DAddr, Nx2ListPage> nx2_list_pages;
    unsigned reads{};
    static bool Nx2HasListWords(const Nx2ListPage&, u64, u64) { return false; }
    void Nx2ReadListWords(const Nx2ListPage& page, DAddr, u64, std::span<u8> out) {
        assert(runtime.IsFree(page.copy_tick));
        ++reads;
        out.front() = page.value;
    }
'''

checks = r'''
};

void TestSuspension() {
    using Result = Cache::Nx2ListRead;
    Cache cache;
    auto& pages = cache.nx2_list_pages;
    pages[0x1000] = {};
    pages[0x2000].copy_tick = 4;
    pages[0x2000].value = 0x2a;
    std::array<u8, 1> output{0xa5};
    Tegra::PendingListCopy pending{};
    for (int attempt = 0; attempt < 4; ++attempt) {
        assert(cache.Nx2ReadListCopy(0x1060, output, false, &pending) == Result::Pending);
        assert(pending.tick == 12 && pending.writer == 51);
        assert(cache.runtime.waits == 0 && cache.reads == 0);
        assert(output.front() == 0xa5);
    }
    Tegra::PendingListCopy other_channel{};
    assert(cache.Nx2ReadListCopy(0x2000, output, false, &other_channel) == Result::Copied);
    assert(output.front() == 0x2a && cache.runtime.waits == 0 && cache.reads == 1);
    cache.runtime.done = 12;
    assert(cache.Nx2ReadListCopy(0x1060, output, false, &pending) == Result::Copied);
    assert(output.front() == 0x42 && cache.reads == 2);
}

void TestRevalidation() {
    using Result = Cache::Nx2ListRead;
    Cache cache;
    auto& pages = cache.nx2_list_pages;
    pages[0x1000] = {};
    std::array<u8, 1> output{0xa5};
    Tegra::PendingListCopy pending{};
    auto read = [&] { return cache.Nx2ReadListCopy(0x1000, output, false, &pending); };
    assert(read() == Result::Pending);
    cache.runtime.done = 12;
    pages[0x2000] = pages[0x1000];
    assert(cache.Nx2ReadListCopy(0x2000, output, false, &pending) == Result::Unknown);
    ++pages[0x1000].writes;
    assert(read() == Result::Unknown);
    pages[0x1000].copy_writes = pages[0x1000].writes;
    assert(read() == Result::Unknown);
    pages[0x1000].copy_tick = 13;
    cache.runtime.submitted = 13;
    assert(read() == Result::Unknown);
    pending = {};
    assert(read() == Result::Pending && pending.tick == 13);
    cache.runtime.on_probe = [&] {
        cache.runtime.done = 13;
        ++pages[0x1000].writes;
    };
    assert(read() == Result::Unknown && cache.reads == 0);
    pages.erase(0x1000);
    assert(read() == Result::Unknown && cache.reads == 0 && output.front() == 0xa5);
}

void TestBlockingFallbacks() {
    using Result = Cache::Nx2ListRead;
    Cache cache;
    auto& page = cache.nx2_list_pages[0x1000];
    std::array<u8, 1> output{0xa5};
    Tegra::PendingListCopy pending{};
    page.copy_tick = cache.runtime.CurrentTick();
    assert(cache.Nx2ReadListCopy(0x1000, output, false, &pending) == Result::Copied);
    assert(cache.runtime.waits == 1 && output.front() == 0x42);
    page.copy_tick = cache.runtime.submitted = 15;
    cache.runtime.on_wait = [&page] { ++page.writes; };
    assert(cache.Nx2ReadListCopy(0x1000, output, false, nullptr) == Result::Unknown);
    assert(cache.runtime.waits == 2 && cache.reads == 1);
}

void TestPollRate() {
    using namespace std::chrono_literals;
    Vulkan::ListTickPoll poll;
    const auto now = std::chrono::steady_clock::time_point{} + 1s;
    assert(poll.ShouldRefresh(now));
    assert(!poll.ShouldRefresh(now));
    assert(!poll.ShouldRefresh(now + 19us));
    assert(poll.ShouldRefresh(now + 20us));
    assert(!poll.ShouldRefresh(now + 21us));
    assert(poll.ShouldRefresh(now + 2s));
    Vulkan::ListTickPoll next_session;
    assert(next_session.ShouldRefresh(now + 2s + 1us));
}

void TestScheduler();

int main() {
    TestSuspension();
    TestRevalidation();
    TestBlockingFallbacks();
    TestPollRate();
    TestScheduler();
    std::cout << "PASS: incomplete and completed copies, interleaved reads, "
                 "stale page checks, blocking fallbacks, bounded polling and scheduler barriers\n";
}
'''

scheduler_fixture = r'''
struct GPU {};
struct FakeSystem {
    bool powered_on = true;
    bool IsPoweredOn() const { return powered_on; }
};
struct Dma {
    FakeSystem system;
    bool copy{}, guest{}, ready{};
    unsigned dispatches{}, waits{};
    bool IsSuspended() const { return copy || guest; }
    bool HasPendingAcquire() const { return guest; }
    bool CanResumeAcquire() const { return ready; }
    bool HasPendingListRead() const { return copy; }
    void DispatchCalls(bool allow_yield = true) {
        assert(ready);
        assert((waits == 0) == allow_yield);
        ++dispatches;
        copy = guest = ready = false;
    }
    void WaitForPendingListRead() {
        assert(copy);
        ++waits;
        ready = true;
    }
};
struct Payload { Dma dma_pusher; };
struct Channel {
    int bind_id;
    std::unique_ptr<Payload> payload = std::make_unique<Payload>();
};
struct Scheduler {
    std::vector<std::shared_ptr<Channel>> pending_channels;
    std::atomic<bool> any_pending{true};
    std::vector<int> binds;
    void BindChannel(GPU&, int id) { binds.push_back(id); }
    void ResumePending(GPU&);
    void FinishPendingListReads(GPU&);
    void RefreshPendingChannels();
};
'''

scheduler_checks = r'''
void TestScheduler() {
    GPU gpu;
    Scheduler scheduler;
    auto blocked = std::make_shared<Channel>(Channel{1});
    auto guest = std::make_shared<Channel>(Channel{2});
    auto released = std::make_shared<Channel>(Channel{3});
    auto& blocked_dma = blocked->payload->dma_pusher;
    auto& guest_dma = guest->payload->dma_pusher;
    auto& released_dma = released->payload->dma_pusher;
    blocked_dma.copy = true;
    guest_dma.guest = true;
    released_dma.copy = released_dma.ready = true;
    scheduler.pending_channels = {blocked, guest, released};
    scheduler.ResumePending(gpu);
    assert(scheduler.binds == std::vector<int>{3});
    assert(blocked_dma.dispatches == 0 && blocked_dma.waits == 0);
    assert(guest_dma.dispatches == 0 && guest_dma.waits == 0);
    assert(scheduler.pending_channels.size() == 2 && scheduler.any_pending);
    scheduler.FinishPendingListReads(gpu);
    assert(blocked_dma.dispatches == 1 && blocked_dma.waits == 1);
    assert(guest_dma.dispatches == 0 && guest_dma.waits == 0);
    assert(scheduler.pending_channels.size() == 1 && scheduler.any_pending);
    guest_dma.ready = true;
    guest_dma.system.powered_on = false;
    scheduler.ResumePending(gpu);
    assert(guest_dma.dispatches == 0 && scheduler.any_pending);
    guest_dma.system.powered_on = true;
    scheduler.ResumePending(gpu);
    assert(guest_dma.dispatches == 1 && scheduler.pending_channels.empty());
    assert(!scheduler.any_pending);
    blocked_dma.copy = true;
    scheduler.pending_channels = {blocked};
    scheduler.ResumePending(gpu);
    assert(scheduler.pending_channels.size() == 1 && !scheduler.any_pending);
}
'''

def scheduler_method(name):
    source = (root / "src/video_core/control/scheduler.cpp").read_text()
    start = source.index(f"void Scheduler::{name}(")
    end = source.index("\n}", start) + 2
    return source[start:end] + "\n"


cpp = out / "list-copy-wait.cpp"
exe = out / "list-copy-wait.exe"
cpp.write_text(fixture + read_result + "\n" + reader + checks + scheduler_fixture
               + scheduler_method("ResumePending") + scheduler_method("FinishPendingListReads")
               + scheduler_method("RefreshPendingChannels") + scheduler_checks)
subprocess.run([
    "clang-cl", "/nologo", "/std:c++20", "/EHsc", "/O2", "/W4", "/WX", "/UNDEBUG",
    "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(root / "src"), str(cpp), "/Fe:" + str(exe),
    "/Fo:" + str(out / "list-copy-wait.obj"),
], check=True)
subprocess.run([str(exe)], check=True, timeout=20)
