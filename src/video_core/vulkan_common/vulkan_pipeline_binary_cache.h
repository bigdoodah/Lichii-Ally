// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

/// Saves the driver's compiled pipelines with VK_KHR_pipeline_binary and gives them back to the
/// driver on later runs to avoid repeated compilation when a saved binary is accepted.
///
/// Only AMD's proprietary driver enables this store (Device::RemoveUnsuitableExtensions):
/// upstream #1406 reports AMD's internal cache misses dynamic-vertex-input graphics pipelines.
/// This store retains their finished binaries without replacing the shader translation cache.
///
/// One file per title, beside the title's other caches:
///   <shader dir>/<title id>/vulkan_binaries-ally.fork-v<cache version>.bin
/// The file records the driver's global pipeline key; a driver update replaces it. Removing the
/// title's shader caches from the game list removes it too. A binary the driver rejects is
/// compiled and saved again, and the file is rewritten without the replaced records once they
/// take a quarter of it (and 64 MiB), or with the pipelines the session used first once it exceeds
/// its 1 GiB target (up to 4 GiB of session-used binaries).
namespace PipelineBinaryCache {

/// Starts saving and reusing binaries for the pipelines created on this device. No-op when the
/// device doesn't have VK_KHR_pipeline_binary enabled.
bool Activate(VkDevice device, const vk::DeviceDispatch& dld, const std::filesystem::path& file);

/// Stops it and closes the file. Pipelines created afterwards are built normally.
void Deactivate();

/// Called by vk::Device. Returns false when the pipeline should be created the usual way.
bool CreateGraphicsPipeline(VkDevice device, const vk::DeviceDispatch& dld,
                            const VkGraphicsPipelineCreateInfo& ci, VkPipelineCache cache,
                            VkPipeline* out, VkResult* result);
bool CreateComputePipeline(VkDevice device, const vk::DeviceDispatch& dld,
                           const VkComputePipelineCreateInfo& ci, VkPipelineCache cache,
                           VkPipeline* out, VkResult* result);

} // namespace PipelineBinaryCache

} // namespace Vulkan
