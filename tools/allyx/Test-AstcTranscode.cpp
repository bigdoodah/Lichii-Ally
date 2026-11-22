// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "video_core/renderer_vulkan/vk_astc_transcode.h"

using namespace Vulkan::AstcTranscode;

void Check(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    for (const char* value :
         std::array<const char*, 8>{nullptr, "", "0", "true", "01", "10", "1 ", "-1"}) {
        Check(!LowMemoryEnabled(value, true));
    }
    Check(LowMemoryEnabled("1", true));
    Check(!LowMemoryEnabled("1", false));

    Check(DecodedLayerBytes(1, 1, 1) == 4);
    Check(DecodedLayerBytes(7, 5, 3) == (35 + 6 + 1) * 4);
    const u64 oot_layer = DecodedLayerBytes(1365, 1365, 11);
    Check(InitialBatchLayers(90, oot_layer, true) == 23);
    Check(oot_layer * 23 <= MaxDecodeBytes);
    Check(oot_layer * 45 > MaxDecodeBytes);
    Check(InitialBatchLayers(90, oot_layer, false) == 90);
    Check(InitialBatchLayers(1, MaxDecodeBytes + 1, true) == 1);
    Check(InitialBatchLayers(13, 4, true) == 13);
    Check(InitialBatchLayers(90, std::numeric_limits<u64>::max(), true) == 1);
    Check(CanOffsetLayers(65536, 256));
    Check(!CanOffsetLayers(260, 256));
    Check(!CanOffsetLayers(0, 256));
    Check(!CanOffsetLayers(65536, 0));

    // An allocation failure halves to a single layer without losing an odd remainder.
    u32 fallback = 90;
    for (const u32 expected : {45U, 23U, 12U, 6U, 3U, 2U, 1U}) {
        fallback = Common::DivCeil(fallback, 2U);
        Check(fallback == expected);
    }

    // BC3 keeps the alpha block: 16 bytes per 4x4 block, vs 8 for opaque BC1.
    Check(EncodedLayerBytes(1, 1, false) == 8);
    Check(EncodedLayerBytes(1, 1, true) == 16);
    Check(EncodedLayerBytes(7, 5, false) == 32);
    Check(EncodedLayerBytes(7, 5, true) == 64);
    for (const bool bc3 : {false, true}) {
        for (const auto shape : {std::array{1U, 1U, 1U}, std::array{7U, 5U, 3U},
                                 std::array{1365U, 1365U, 11U}}) {
            // Every mip's batched output must exactly partition its original layer-major range.
            u64 mip_offset{};
            for (u32 level = 0; level < shape[2]; ++level) {
                const u32 width = (std::max)(shape[0] >> level, 1U);
                const u32 height = (std::max)(shape[1] >> level, 1U);
                const u64 bytes = EncodedLayerBytes(width, height, bc3);
                u64 reference_blocks{};
                for (u32 y = 0; y < height; y += 4) {
                    for (u32 x = 0; x < width; x += 4) {
                        ++reference_blocks;
                    }
                }
                Check(bytes == reference_blocks * (bc3 ? 16 : 8));
                u64 next = mip_offset;
                u32 visited{};
                for (u32 base = 0; base < 90; base += 23) {
                    const u32 count = (std::min)(23U, 90U - base);
                    Check(mip_offset + bytes * base == next);
                    Check((mip_offset + bytes * base) % 8 == 0);
                    next += bytes * count;
                    visited += count;
                }
                Check(visited == 90);
                Check(next == mip_offset + bytes * 90);
                mip_offset = next;
            }
        }
    }

    // The last layer may omit trailing alignment padding; no batch reads beyond its upload.
    constexpr u64 stride = 65536;
    constexpr u64 guest_size = stride * 90 - 128;
    u64 consumed{};
    for (u32 base = 0; base < 90; base += 23) {
        const u32 count = (std::min)(23U, 90U - base);
        const u64 bytes = BatchInputBytes(guest_size, stride, base, count);
        Check(consumed == u64{base} * stride);
        consumed += bytes;
        Check(consumed <= guest_size);
    }
    Check(consumed == guest_size);
    Check(BatchInputBytes(guest_size, stride, 90, 1) == 0);
    Check(BatchInputBytes(0, stride, 0, 1) == 0);
    std::cout << "ASTC transcode: strict opt-in, scratch cap, alignment fallback, odd mips, "
                 "BC1/BC3 layout, remainder layers and input bounds passed\n";
}
