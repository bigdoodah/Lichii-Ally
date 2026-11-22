// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>

#include "video_core/nx2.h"

namespace VideoCommon::QueryDma {

// The tracker records each report's starting slot. A timestamped report writes 16 bytes.
struct ReportReadRange {
    u64 begin;
    u64 size;

    static constexpr ReportReadRange FromRead(u64 address, u64 bytes) {
        const u64 prefix = bytes ? (std::min)(address, u64{15}) : 0;
        return {address - prefix, bytes + prefix};
    }

    bool NeedsFence(bool cache_needs_results, bool wait_own_reports) const {
        // SemiFlushRegion can write the value before the callback writes the timestamp. It
        // also indexes only the first word, so a read of the tail can miss the cached report.
        return cache_needs_results ||
               (wait_own_reports && !Tegra::Nx2AsyncReportsSettled(begin, size));
    }
};

} // namespace VideoCommon::QueryDma
