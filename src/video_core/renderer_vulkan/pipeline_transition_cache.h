// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

namespace Vulkan {

// GPU-thread-only shortcuts. A generation change invalidates pointers before any lookup or add.
template <typename Key, typename Pipeline>
class PipelineTransitionCache {
public:
    [[nodiscard]] Pipeline* Find(const Key& key, std::uint64_t generation) noexcept {
        Refresh(generation);
        const auto it = std::find(keys.begin(), keys.end(), key);
        return it != keys.end() ? pipelines[std::distance(keys.begin(), it)] : nullptr;
    }

    void Add(const Key& key, Pipeline* pipeline, std::uint64_t generation) {
        Refresh(generation);
        keys.push_back(key);
        pipelines.push_back(pipeline);
    }

    void Clear() noexcept {
        keys.clear();
        pipelines.clear();
    }

private:
    void Refresh(std::uint64_t generation) noexcept {
        if (generation != current_generation) {
            Clear();
            current_generation = generation;
        }
    }

    std::uint64_t current_generation{};
    std::vector<Key> keys;
    std::vector<Pipeline*> pipelines;
};

} // namespace Vulkan
