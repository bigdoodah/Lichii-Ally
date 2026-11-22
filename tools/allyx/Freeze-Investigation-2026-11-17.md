# Ally X freeze investigation — 17 November 2026

## Decision

Use Performance6's existing asynchronous buffer readback implementation for one reversible comparison (`EDEN_NX2_EXP49=1`). Keep the inline timing counters. Do not remove synchronization, disable reactive flushing, change the native motion engine, or import a large rendering branch. This is a candidate optimization, not a demonstrated fix for the multi-second freezes. No executable rebuild is necessary.

The launch environment is process-local. Starting lichii.exe normally returns to its existing defaults, provided no external environment override is present. The diagnostic launcher is the only changed launch path.

## What the user's captures establish

- Performance6 build: master-1f00d28f4a.
- The targeted run recorded worst frames of 2329.5 and 2771.9 ms, with zero recorded draw hold time for shader pipelines. The former built one pipeline in 0.4 ms; the latter built none.
- Command-list writer tracing reported a maximum individual wait of 108909 us in the inspected capture. The main writer was 541ba22d44898196. The sampled reports showed zero unsubmitted waits. This does not support treating one command-list semaphore wait as the entire multi-second freeze.
- Near the 2329.5 ms frame, the back-thread profile reported read flushes at 281.4 ms/s over a 10000 ms window. This is aggregate time, not attribution of that frame. The separate readback counter reported synchronous reads only, confirming that async mode was not active.
- Another multi-second frame did not show the same elevated read-flush total. There may be more than one cause. Free-memory readings also vary; free RAM alone does not prove or exclude paging or driver residency pressure.
- Logging gaps are not proof of a stalled thread: idle periods naturally produce fewer log messages.

## Research comparison

### Lichii #1028: async readback (merged)

Source: https://github.com/zortos293/Lichii/pull/1028

The old route waited for the host GPU on the GPU thread while holding the buffer-cache lock. The replacement submits copies and lets the reading guest thread wait outside that lock. Other GPU work can proceed, and overlapping reads can share in-flight copies. The PR includes fixes preserving pending CPU writes and ordering DMA/query writes correctly.

The developer reported OoT Kokiri mean FPS rising from 28.2 to 32.9 on a Ryzen 5900X / RTX 3080, with a 5 ms p90 frame-time improvement. Bananza was essentially unchanged. These results are not an Ally X benchmark and do not establish a cure for seconds-long stalls.

Local inspection: gpu.cpp OnCPURead selects this route through Experiment(49); it is off by default. RasterizerVulkan::FlushRegionForRead calls DownloadMemoryForRead, and FinishReadDownloads releases the cache mutex before WaitForHostRead. The local header explicitly identifies this as a partial port: it omits upstream early downloads and READ_CHECK verification. Consequently the full PR's benchmark and validation cannot be transferred to this build.

### Lichii #861 and #147: preflushing and batching

Sources:
- https://github.com/zortos293/Lichii/pull/861
- https://github.com/zortos293/Lichii/pull/147

#861 addresses repeated reads of the same GPU-written pages. Its predecessor gained speed but flickered because reads could observe data before a pending download landed. The corrected proposal waits for completion and preserves read-trap rearming. It remained open when inspected; its gameplay testing was Linux/NVIDIA and limited to Kokiri.

#147 proposed combining learned readback ranges into one wait. It was closed without merging. Historical integration results improved FPS and reduced Finish calls, but the independent corrected branch had no fresh gameplay benchmark. The local async path already includes learned-range gathering, so blindly importing this patch is inappropriate.

Decision: use the available async path first; do not turn on unsafe preflushing or claim that batching is newly implemented here.

### Eglantine: useful negative results

Sources:
- https://github.com/Eglantine-Community/Eglantine/pull/686
- https://github.com/Eglantine-Community/Eglantine/pull/151
- https://github.com/Eglantine-Community/Eglantine/pull/609
- https://github.com/Eglantine-Community/Eglantine/pull/700

#686 reports four ways of throttling readbacks to once per frame producing washed-out or flickering output; the eager transfers were FPS-neutral in that measured case. #151 reports that bypassing reactive read traps reduced MKW performance and increased long frames, besides allowing stale reads. #609 reports large gains from narrowing native bindings, but #700 proposes reverting it. Large performance claims therefore require checking ordering, scope, and later reversals.

Decision: do not disable reactive flushing, throttle freshness to once a frame, or narrow shader bindings speculatively.

### Related OoT freeze fixes that do not yet match our evidence

- https://github.com/zortos293/Lichii/pull/2052: syncpoint notification fix, ported from an Eglantine fork/Orchid. A rearmed wait can retain the same armed bit while its value advances. The notification bug has a targeted regression reproduction, but the reported one-second gameplay freeze was not reproduced. PR remains held for testing. Worth retaining as the next correctness candidate if waits persist; not yet established as our cause.
- https://github.com/Eglantine-Community/Eglantine/pull/712: delayed-fence ring deadlock. Describes all channels permanently parked behind semaphores. Our captured pauses recover and do not establish that signature. Do not substitute semaphore values based on this report.
- https://github.com/zortos293/Lichii/pull/2012: mesh pipeline key deduplication addresses first-effect compilation freezes. Our specific frozen frames show negligible/no pipeline build or draw hold time, so this is not the first target.

### Switch 1: yuzu's reactive flushing and memory management

Primary developer report, preserved mirror:
https://yuzu-mirror.github.io/entry/yuzu-progress-report-may-2023/

yuzu documented that full synchronous flushing was safe but slow; predictive flushing improved performance but caused incorrect lighting, shadows, thumbnails, and geometry. Its later reactive-flushing work combined correctness with asynchronous Vulkan buffers. The same report documents memory-pressure remedies: cache headroom, integrated-GPU memory caps, and ASTC recompression. The dedicated-GPU PCIe explanation in that report must not be applied literally to the Ally's shared-memory architecture.

Decision: follow the asynchronous-but-correct approach. Memory-budget tuning remains secondary until residency/collection costs are attributable. Do not blindly copy a Switch 1 memory cap to a Switch 2 workload.

### Switch 1: Ryujinx staging cleanup and submit latency

Preserved original commit descriptions:
- https://git.axenov.dev/Museum/ryujinx/commits/commit/319507f2a12a6751f3ab833e498a3efd3119f806/src/Ryujinx.Graphics.Vulkan/VulkanRenderer.cs
- https://git.axenov.dev/Museum/ryujinx/commits/commit/2ca70eb9a05e057ebc2122bd749c49b4dea5c523/search?q=author%3Arhy3756547%40hotmail.com

Ryujinx #5572 (0c445184c1) fixed an edge case where thousands of staging-copy fences accumulated until buffer exhaustion caused a large cleanup stutter. It checked completion on command-buffer submission. Another recorded change periodically submitted Vulkan commands to reduce readback latency, improving Pokemon Sword and some BOTW areas.

Local inspection: Lichii's staging pool already calls ReleaseCache from TickFrame and falls back to another staging allocation when stream regions are busy. It is not the same implementation, so copying Ryujinx's patch is not justified. The general lesson is to bound accumulated work and inspect submit/retirement latency. Performance5 already bounds pipeline destruction; it does not prove all staging/texture cleanup is bounded.

### Reddit: similar symptoms, conflicting resolutions

- https://www.reddit.com/r/yuzu/comments/16e7xqw : Steam Deck TOTK menu freezes of 4–5 seconds. Suggestions included ASTC recompression, while another user reported eliminating freezes by changing BC3 back to uncompressed.
- https://www.reddit.com/r/yuzu/comments/156x3rx : reports connect lower texture memory usage with menu-exit stalls or async-decoding artifacts.
- https://www.reddit.com/r/ROGAlly/comments/19be8p2 : an Ally owner reports improved smoothness after changing VRAM allocation to Auto. Other Ally discussions recommend fixed allocations instead.

These are firsthand symptom/workaround reports, not controlled proof. They show that similar pauses can have different causes and that memory-saving settings can trade one bottleneck for another. No blanket VRAM, ASTC, or driver change follows from them.

## One bounded comparison

Launch through the updated Trace-Frame-Timing.cmd after closing the current session normally. Play the same saved area and ordinary route for a few minutes. No repeated pause announcements are needed.

Inspect the log for nonzero async reads, reduced GPU-thread read-flush time, long-frame frequency/duration, and visual correctness. Async mode can still synchronously flush textures and query data, so remaining stalls are possible. If the result is neutral or causes artifacts, return to normal lichii.exe and reject this mode for now. The next code candidate is notification correctness or bounded cleanup, selected from the remaining timing signature—not another bundle of unrelated tweaks.

## Validation status

Research includes upstream PR bodies/status and selected diffs, Switch 1 developer history, Reddit reports, and local source paths. No new rendering code was compiled or tested. Only the process-local diagnostic launch environment was changed. Gameplay improvement remains unmeasured.
