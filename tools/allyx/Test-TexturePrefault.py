"""Test the production prefault helper with Windows guard pages and real workers.

Run in a configured clang-cl/MSVC environment:
    python Test-TexturePrefault.py OUTPUT_DIRECTORY

Only memory translation, profiling and thread affinity are stubbed. This checks
page access, memory preservation and worker lifetime; it is not a speed benchmark.
"""
from pathlib import Path
import subprocess
import sys


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
production = (root / "src/video_core/texture_cache/util.cpp").read_text()
helper = function(production, "void PrefaultGuestMemory(")

prefix = r'''
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <latch>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>
#include "common/thread_worker.h"

namespace Common {
void SetCurrentThreadPriority(ThreadPriority) {}
void SetCurrentThreadName(const char*) {}
void SetCurrentThreadToEfficiencyCores() {}
void SetCurrentThreadToBackgroundWork() {}
void SetCurrentThreadToAllCores() {}
template <typename T> constexpr T AlignDown(T value, size_t align) {
    return value & ~static_cast<T>(align - 1);
}
template <typename T> constexpr T DivCeil(T value, T divisor) {
    return value / divisor + (value % divisor != 0);
}
namespace Literals {
constexpr size_t operator""_KiB(unsigned long long n) { return n * 1024; }
constexpr size_t operator""_MiB(unsigned long long n) { return n * 1024 * 1024; }
}
}
namespace Nx2Profile {
enum class Zone { TexturePrefault };
struct Scope { explicit Scope(Zone) {} };
}
constexpr size_t Page = 4096, MiB = 1024 * 1024;
constexpr GPUVAddr Guest = 0x100000000ULL;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Admission delegates to the production worker. The failure hook simulates a
// task allocation failing after an earlier helper already captured local state.
struct WorkerAccess {
    Common::ThreadWorker real{2, "PrefaultTest"};
    int attempts{}, fail_at{-1};
    bool failure_seen{};
    template <typename F> bool TryQueueWork(F&& task) {
        if (attempts++ == fail_at) {
            failure_seen = true;
            throw std::bad_alloc{};
        }
        return real.TryQueueWork(std::forward<F>(task));
    }
    void Reset() { real.WaitForRequests(); attempts = 0; fail_at = -1; failure_seen = false; }
};
namespace Tegra {
struct MemoryManager {
    std::vector<u8*> pages;
    bool contiguous = true;
    size_t span_queries{}, pointer_queries{};
    const u8* GetSpan(GPUVAddr address, size_t size) {
        ++span_queries;
        if (!contiguous || address < Guest || address - Guest + size > pages.size() * Page)
            return nullptr;
        return pages[0] + (address - Guest);
    }
    const u8* GetPointer(GPUVAddr address) {
        ++pointer_queries;
        if (address < Guest || (address - Guest) / Page >= pages.size()) return nullptr;
        const auto index = (address - Guest) / Page;
        return pages[index] ? pages[index] + (address - Guest) % Page : nullptr;
    }
};
namespace Texture {
WorkerAccess& GetThreadWorkers() { static WorkerAccess workers; return workers; }
}
}
'''

suffix = r'''
struct GuardTracker {
    u8* begin{};
    size_t count{};
    std::unique_ptr<std::atomic<unsigned>[]> reads;
    std::atomic<unsigned> writes{};
};
GuardTracker* tracker{};
LONG CALLBACK GuardHandler(EXCEPTION_POINTERS* exception) {
    if (exception->ExceptionRecord->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION || !tracker)
        return EXCEPTION_CONTINUE_SEARCH;
    auto* address = reinterpret_cast<u8*>(exception->ExceptionRecord->ExceptionInformation[1]);
    if (address < tracker->begin || address >= tracker->begin + tracker->count * Page)
        return EXCEPTION_CONTINUE_SEARCH;
    const auto index = static_cast<size_t>(address - tracker->begin) / Page;
    tracker->reads[index].fetch_add(1);
    if (exception->ExceptionRecord->ExceptionInformation[0] != 0) ++tracker->writes;
    return EXCEPTION_CONTINUE_EXECUTION;
}

struct MemoryFixture {
    u8* allocation{};
    u8* data{};
    size_t count;
    std::vector<u8> before;
    Tegra::MemoryManager memory;
    GuardTracker trace;
    std::vector<bool> expected;
    explicit MemoryFixture(size_t pages) : count(pages), before(pages * Page), expected(pages) {
        allocation = static_cast<u8*>(VirtualAlloc(nullptr, (pages + 2) * Page, MEM_RESERVE, PAGE_NOACCESS));
        Require(allocation != nullptr, "reserve guarded memory");
        data = static_cast<u8*>(VirtualAlloc(allocation + Page, pages * Page, MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "commit guarded memory");
        for (size_t i = 0; i < before.size(); ++i) before[i] = static_cast<u8>(i * 37 + i / Page);
        std::memcpy(data, before.data(), before.size());
        for (size_t i = 0; i < pages; ++i) memory.pages.push_back(data + i * Page);
        trace.begin = data; trace.count = count;
        trace.reads = std::make_unique<std::atomic<unsigned>[]>(count);
    }
    ~MemoryFixture() { tracker = nullptr; if (allocation) VirtualFree(allocation, 0, MEM_RELEASE); }
    void Protect(size_t index, DWORD protection) {
        DWORD previous{};
        Require(VirtualProtect(data + index * Page, Page, protection, &previous) != 0, "protect page");
    }
    void Arm(size_t offset, size_t bytes) {
        for (size_t page = offset / Page; page <= (offset + bytes - 1) / Page; ++page) {
            if (memory.pages[page]) expected[(memory.pages[page] - data) / Page] = true;
        }
        for (size_t i = 0; i < count; ++i)
            Protect(i, expected[i] ? PAGE_READONLY | PAGE_GUARD : PAGE_NOACCESS);
        tracker = &trace;
    }
    void Check() {
        // Protect immediately, before waiting on the pool: unfinished accesses
        // after PrefaultGuestMemory returned must fail instead of passing silently.
        for (size_t i = 0; i < count; ++i) Protect(i, PAGE_NOACCESS);
        Tegra::Texture::GetThreadWorkers().real.WaitForRequests();
        Require(trace.writes == 0, "prefault wrote guest bytes");
        for (size_t i = 0; i < count; ++i) {
            Require(trace.reads[i] == static_cast<unsigned>(expected[i]), "missing or extra page touch");
            Protect(i, PAGE_READONLY);
        }
        Require(std::memcmp(data, before.data(), before.size()) == 0, "guest bytes changed");
        tracker = nullptr;
    }
};

void Case(const char* label, size_t offset, size_t bytes, bool fragmented = false, bool all_holes = false) {
    MemoryFixture fixture((offset + bytes + Page - 1) / Page + 1);
    fixture.memory.contiguous = !fragmented;
    if (fragmented) {
        std::reverse(fixture.memory.pages.begin(), fixture.memory.pages.end());
        for (size_t i = 0; i < fixture.count; ++i)
            if (all_holes || i % 7 == 0) fixture.memory.pages[i] = nullptr;
    }
    fixture.Arm(offset, bytes);
    PrefaultGuestMemory(fixture.memory, Guest + offset, bytes);
    fixture.Check();
    std::cout << "PASS: " << label << '\n';
}

int main() {
    auto* handler = AddVectoredExceptionHandler(1, GuardHandler);
    if (!handler) return 1;
    try {
        auto& workers = Tegra::Texture::GetThreadWorkers();
        Tegra::MemoryManager empty;
        PrefaultGuestMemory(empty, Guest, 0);
        PrefaultGuestMemory(empty, Guest, MiB - 1);
        Require(empty.span_queries == 0 && empty.pointer_queries == 0, "small range accessed memory");
        std::cout << "PASS: below threshold performs no translations\n";
        Case("aligned contiguous, adjacent inaccessible pages", 0, MiB);
        Case("unaligned beginning and ending", 1, MiB + 7);
        Case("unaligned beginning, page-aligned ending", Page - 1, MiB + 1);
        Case("fragmented reverse mappings and holes", 17, MiB + Page + 3, true);
        Case("fully unmapped range", 0, MiB, true, true);

        workers.Reset();
        workers.fail_at = 1;
        Case("allocation failure after first admitted helper", 0, 2 * MiB);
        Require(workers.failure_seen, "admission fault was not exercised");
        workers.Reset();

        // Saturate the real pool with unfinished decode-like jobs. The helper
        // must complete on the caller without waiting for those jobs to finish.
        std::latch started{2}, release{1};
        for (int i = 0; i < 2; ++i)
            workers.real.QueueWork([&] { started.count_down(); release.wait(); });
        started.wait();
        MemoryFixture busy(257);
        busy.Arm(0, MiB);
        auto attempt = std::async(std::launch::async, [&] {
            PrefaultGuestMemory(busy.memory, Guest, MiB);
        });
        const bool completed = attempt.wait_for(std::chrono::seconds(3)) == std::future_status::ready;
        release.count_down();
        attempt.get();
        Require(completed, "prefault waited behind busy workers");
        busy.Check();
        std::cout << "PASS: saturated production worker pool falls back to caller\n";
        RemoveVectoredExceptionHandler(handler);
        std::cout << "PASS: extracted production helper; guard pages, read-only data, real workers\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        RemoveVectoredExceptionHandler(handler);
        return 1;
    }
}
'''

cpp = out / "texture-prefault.cpp"
exe = out / "texture-prefault.exe"
cpp.write_text(prefix + helper + suffix)
subprocess.run([
    "clang-cl", "/nologo", "/std:c++20", "/EHsc", "/O2", "/W4", "/WX",
    "/I" + str(root / "src"), str(cpp), "/Fe:" + str(exe),
    "/Fo:" + str(out / "texture-prefault.obj"),
], check=True)
subprocess.run([str(exe)], check=True, timeout=30)
