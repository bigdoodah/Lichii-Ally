"""Compile the actual selection methods against controlled cache state; no emulator launch."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--out', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = Path(args.out).resolve()
out.mkdir(parents=True, exist_ok=True)
relative = 'src/video_core/renderer_vulkan/vk_pipeline_cache.cpp'
current = (root / relative).read_text()
previous = subprocess.check_output(['git', '-C', str(root), 'show',
                                   '2b10f3b:' + relative], text=True)
def selection(text):
    return text[text.index('void PipelineCache::EvictUnusedPipelines() {'):
                text.index('void PipelineCache::DrainPendingGraphicsEvictions() {')]
code = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <vector>
#include "video_core/texture_cache/collection_budget.h"
using u64 = uint64_t;
#define LOG_INFO(...)
namespace VideoCommon {
size_t GraphicsPipelinesToEvict(size_t cached, size_t stale, size_t limit) {
    return cached <= limit ? 0 : std::min(stale, cached - limit);
}
namespace FrameEndTrace { struct Scope { Scope(const char*) {} }; }
}
struct GraphicsPipelineCacheKey {
    int id{};
    static inline size_t copies{};
    GraphicsPipelineCacheKey(int value) : id(value) {}
    GraphicsPipelineCacheKey(const GraphicsPipelineCacheKey& other) : id(other.id) { ++copies; }
    GraphicsPipelineCacheKey& operator=(const GraphicsPipelineCacheKey& other) {
        id = other.id; ++copies; return *this;
    }
    bool operator<(const GraphicsPipelineCacheKey& other) const { return id < other.id; }
};
struct Pipeline {
    bool built = true, used = true;
    u64 last{};
    bool IsBuilt() const { return built; }
    bool WasUsed() const { return used; }
    u64 LastUse() const { return last; }
};
struct PipelineCache {
    struct Device { bool molten{}; bool IsMoltenVK() const { return molten; } } device;
    bool preload{};
    bool PreloadAllPipelines() const { return preload; }
    size_t limit = 2500;
    size_t EvictAbovePipelines() const { return limit; }
    u64 frame_count = 20000, pending_eviction_cutoff{};
    size_t pending_eviction_removed{}, pending_eviction_frames{};
    Pipeline* current_pipeline{};
    std::map<GraphicsPipelineCacheKey, std::unique_ptr<Pipeline>> graphics_cache;
    std::deque<std::pair<u64, GraphicsPipelineCacheKey>> pending_graphics_evictions;
    void EvictUnusedPipelines();
    void Original();
};
'''
code += selection(current)
code += selection(previous).replace('PipelineCache::EvictUnusedPipelines()', 'PipelineCache::Original()')
code += r'''
std::vector<int> Selected(const PipelineCache& cache) {
    std::vector<int> result;
    for (const auto& entry : cache.pending_graphics_evictions) result.push_back(entry.second.id);
    std::sort(result.begin(), result.end());
    return result;
}
int main() {
    using namespace std::chrono_literals;
    using Budget = VideoCommon::CollectionBudget;
    const Budget::Clock::time_point start{};
    Budget normal(start, false, false);
    assert(!normal.Exhausted(start + 1h)); // slow selection cannot starve the first eligible image
    normal.StartImage();
    assert(!normal.Exhausted(start + 1999us));
    assert(normal.Exhausted(start + 2ms));
    Budget pressure(start, false, true);
    pressure.StartImage();
    assert(!pressure.Exhausted(start + 3ms));
    assert(pressure.Exhausted(start + 4ms));
    Budget emergency(start, true, true);
    emergency.StartImage();
    assert(!emergency.Exhausted(start + 1h));
    size_t old_copies{}, new_copies{};
    for (int scenario = 0; scenario < 8; ++scenario) {
        PipelineCache before, after;
        for (auto* cache : {&before, &after}) {
            for (int i = 0; i < 6000; ++i) {
                auto item = std::make_unique<Pipeline>();
                item->last = i;
                if (scenario == 1) item->built = i % 4 == 0;
                if (scenario == 2) item->used = i >= 3000;
                if (scenario == 3) item->last = i < 4000 ? 19999 : i;
                if (scenario == 4 && i % 3 == 0) item.reset();
                cache->graphics_cache.emplace(i, std::move(item));
            }
            cache->current_pipeline = cache->graphics_cache.begin()->second.get();
            if (scenario == 5) cache->preload = true;
            if (scenario == 6) cache->device.molten = true;
            if (scenario == 7) cache->frame_count = 9000;
        }
        GraphicsPipelineCacheKey::copies = 0;
        before.Original();
        old_copies += GraphicsPipelineCacheKey::copies;
        GraphicsPipelineCacheKey::copies = 0;
        after.EvictUnusedPipelines();
        new_copies += GraphicsPipelineCacheKey::copies;
        assert(Selected(before) == Selected(after));
        assert(before.pending_eviction_cutoff == after.pending_eviction_cutoff);
        assert(after.graphics_cache.size() == 6000); // selection never retires live objects
    }
    assert(new_copies < old_copies);
    std::cout << "PASS: 8 real selection comparisons; budget/progress/emergency checks; key copies "
              << old_copies << " -> " << new_copies << '\n';
}
'''
(out / 'checks.cpp').write_text(code)
subprocess.run([args.compiler, '/nologo', '/std:c++20', '/EHsc', '/O2', '/MD',
                '/I' + str(root / 'src'), 'checks.cpp', '/Fe:checks.exe', '/Fo:checks.obj'],
               cwd=out, check=True)
subprocess.run([str(out / 'checks.exe')], check=True)
# Guard the production integration, including sharing the budget between all passes.
texture = (root / 'src/video_core/texture_cache/texture_cache.h').read_text()
assert 'out_of_memory || total_used_memory >= critical_memory,' in texture
assert texture.count('budget.StartImage();') == 1
assert texture.count('budget.Exhausted(CollectionBudget::Clock::now())') == 2
print('PASS: production collection keeps critical/OOM recovery unbudgeted')
