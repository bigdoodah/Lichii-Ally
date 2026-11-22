// SPDX-License-Identifier: GPL-3.0-or-later
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "common/page_table.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/kernel.h"
#include "core/memory.h"
#include "tests/core/system_test_context.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_null/null_rasterizer.h"
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::fprintf(stderr, "Failed line %d\n", __LINE__); std::abort(); } } while (false)
int main() {
    std::fputs("Creating isolated system\n", stderr);
    ScopedSystemTestContext context;
    Core::System system;
    std::fputs("Creating memory and GPU\n", stderr);
    Core::DeviceMemory physical;
    Tegra::MaxwellDeviceMemoryManager device{physical};
    Tegra::MemoryManager memory{system, device, 32, 1ULL << 31};
    Tegra::GPU gpu{system, false, false};
    Null::RasterizerNull rasterizer{gpu};
    std::fputs("Initializing system\n", stderr);
    system.Initialize();
    std::fputs("Mapping test pages\n", stderr);
    Kernel::KProcess process{system.Kernel()};
    SCOPE_EXIT { system.Kernel().UnregisterKernelObject(&process); };
    auto& table = process.GetPageTable().GetImpl();
    table.Resize(20, 12);
    // This fixture bypasses kernel allocation; commit its reserved Windows backing explicitly.
    physical.buffer.Commit(0x10000, 0x1000);
    physical.buffer.Commit(0x30000, 0x1000);
    auto* first = physical.GetPointerFromRaw<u8>(0x10000);
    auto* second = physical.GetPointerFromRaw<u8>(0x30000);
    table.entries.GetAndFault(4).Store(false, Common::PageType::Memory, 1,
                                      reinterpret_cast<uintptr_t>(first) - 0x4000);
    table.entries.GetAndFault(5).Store(false, Common::PageType::Memory, 1,
                                      reinterpret_cast<uintptr_t>(second) - 0x5000);
    process.GetMemory().SetCurrentPageTable(process);
    const auto asid = device.RegisterProcess(&process.GetMemory());
    device.BindInterface(&rasterizer);
    SCOPE_EXIT { device.Unmap(0x10000, 0x2000); device.UnregisterProcess(asid); };
    device.Map(0x10000, 0x4000, 0x2000, asid);
    memory.BindRasterizer(&rasterizer);
    memory.Map(0x100000, 0x10000, 0x2000, Tegra::PTEKind::PITCH, false);
    const auto word = [](const u8* p) { u32 v; std::memcpy(&v, p, 4); return v; };
    u32 value = 0x12345678;
    std::fputs("Checking cached writes\n", stderr);
    CHECK(memory.TryWriteCachedPage(0x100000, &value, 4));
    CHECK(word(first) == value);
    value = 0x87654321;
    CHECK(memory.TryWriteCachedPage(0x100004, &value, 4));
    CHECK(word(first + 4) == value);
    std::fputs("Initial writes passed; remapping GPU page\n", stderr);
    // Changing the GPU mapping must invalidate an otherwise matching cache entry.
    memory.Unmap(0x100000, 0x1000);
    memory.Map(0x100000, 0x11000, 0x1000, Tegra::PTEKind::PITCH, false);
    CHECK(memory.TryWriteCachedPage(0x100004, &value, 4));
    CHECK(word(second + 4) == value);
    std::fputs("GPU remap passed; remapping device backing\n", stderr);
    // A device backing change must invalidate it even when the GPU mapping is unchanged.
    device.Unmap(0x11000, 0x1000);
    device.Map(0x11000, 0x4000, 0x1000, asid);
    value = 0xAABBCCDD;
    CHECK(memory.TryWriteCachedPage(0x100004, &value, 4));
    CHECK(word(first + 4) == value);
    CHECK(word(second + 4) == 0x87654321);
    std::fputs("Device remap passed; checking boundaries and unmap\n", stderr);
    CHECK(!memory.TryWriteCachedPage(0x100fff, &value, 4));
    CHECK(!memory.TryWriteCachedPage(0x100000000ULL, &value, 4));
    memory.Unmap(0x100000, 0x1000);
    CHECK(!memory.TryWriteCachedPage(0x100004, &value, 4));
    std::fputs("All cached-write checks passed; cleaning up\n", stderr);
    std::puts("Passed cached writes, GPU remap, backing remap, boundary and unmap checks");
}
