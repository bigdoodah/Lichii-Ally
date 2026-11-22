// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include "common/common_types.h"
#include "common/div_ceil.h"

namespace Vulkan::AstcTranscode {

constexpr u64 MaxDecodeBytes = u64{256} << 20;

[[nodiscard]] constexpr bool LowMemoryEnabled(const char* value, bool integrated) {
    return integrated && value != nullptr && value[0] == '1' && value[1] == '\0';
}

[[nodiscard]] constexpr u64 DecodedLayerBytes(u32 width, u32 height, u32 levels) {
    u64 bytes{};
    for (u32 level = 0; level < levels; ++level) {
        bytes += u64{(std::max)(width, 1U)} * (std::max)(height, 1U) * 4;
        width >>= 1;
        height >>= 1;
    }
    return bytes;
}

[[nodiscard]] constexpr bool CanOffsetLayers(u64 stride, u64 storage_alignment) {
    return stride != 0 && storage_alignment != 0 && stride % storage_alignment == 0;
}

[[nodiscard]] constexpr u32 InitialBatchLayers(u32 layers, u64 layer_bytes, bool integrated) {
    while (integrated && layers > 1 && layer_bytes > MaxDecodeBytes / layers) {
        layers = Common::DivCeil(layers, 2U);
    }
    return layers;
}

[[nodiscard]] constexpr u64 EncodedLayerBytes(u32 width, u32 height, bool bc3) {
    return u64{Common::DivCeil(width, 4U)} * Common::DivCeil(height, 4U) * (bc3 ? 16 : 8);
}

[[nodiscard]] constexpr u64 BatchInputBytes(u64 guest_size, u64 layer_stride, u32 base,
                                           u32 count) {
    const u64 offset = u64{base} * layer_stride;
    return (std::min)(u64{count} * layer_stride, guest_size - (std::min)(guest_size, offset));
}

} // namespace Vulkan::AstcTranscode
