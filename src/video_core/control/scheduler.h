// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "common/container/unordered_map.h"

#include "video_core/dma_pusher.h"

namespace Tegra {

class GPU;

namespace Control {

struct ChannelState;

class Scheduler {
public:
    void Push(GPU& gpu, s32 channel, CommandList&& entries);

    /// Complete submitted command-list reads before a later host operation, e.g. a GPU map.
    void FinishPendingListReads(GPU& gpu);
    /// Runs channels whose pending acquire or command-list copy is ready.
    void ResumePending(GPU& gpu);
    [[nodiscard]] bool HasPendingChannels() const {
        return !pending_channels.empty();
    }
    /// Guest semaphore suspensions only: the presentation thread may skip its wait for these.
    [[nodiscard]] bool HasPendingChannelsAnyThread() const {
        return any_pending.load(std::memory_order_acquire);
    }

    void DeclareChannel(std::shared_ptr<ChannelState> new_channel);

private:
    void BindChannel(GPU& gpu, s32 bind_id);
    void RefreshPendingChannels();

    ::Common::unordered_map<s32, std::shared_ptr<ChannelState>> channels;
    std::mutex scheduling_guard;
    std::vector<std::shared_ptr<ChannelState>> pending_channels; ///< Suspended on an acquire
    std::atomic<bool> any_pending{};
};

} // namespace Control

} // namespace Tegra
