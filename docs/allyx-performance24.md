# Performance24: correct wait-trace switches and attribute slow stages

Baseline: `dev/performance23-allyx-z1e`, commit `310599cc84`.

## Evidence and rationale

The Performance23 Ocarina capture contains both multi-second shader/pipeline waits and GPU-completion stalls without pipeline builds. A long renderer frame interval alone cannot attribute GPU execution time or prove that a pipeline cache missed.

This patch makes the **diagnostics** more useful without changing rendering, shader compilation scheduling, cache contents, ASTC formats, native motion, or synchronization semantics.

- `EDEN_NX2_LIST_WAIT_TRACE=1` is the only enabling value. `0`, unset, and other strings disable the optional list-submission trace, including its large tick-history buffer and pipeline-wait messages. Previously, mere presence of the variable enabled both paths.
- With that switch enabled, a Vulkan timeline-semaphore wait exceeding 500 ms reports elapsed CPU wall time, the requested tick, last known completed tick on entry, and latest successful queue-submit tick. This **does not** measure actual GPU kernel execution time. Logs are capped at 128 reports per process.
- With `EDEN_NX2_TEXTURE_WAIT_TRACE=1`, `Scheduler::WaitWorker` separates wait for the Vulkan work queue to empty (including dispatch) from the subsequent execution-mutex wait. The two wall-time portions are reported only for waits >=100 ms, under the existing 256-report limit.
- Existing logging and error paths remain present. No new diagnostics are added to the hot path while the switches are off.

## Validation and benchmark plan

A small standalone C++20 smoke test checks the trace-switch parser with enabled/disabled values. Catch2 regression test is registered with `src/tests/CMakeLists.txt` for the full project test build. **Complete emulator build, runtime verification, and Ally X gameplay benchmarking are still required**; the patch does not claim FPS or stutter improvement.

On the separate Performance24 branch, build using `tools/allyx/Build-AllyX.ps1` and retain Performance23 intact. Record logs from the same Hyrule Field route with `EDEN_NX2_LIST_WAIT_TRACE=1`, `EDEN_NX2_TEXTURE_WAIT_TRACE=1`, and `EDEN_NX2_FRAME_END_TRACE=1`. Compare longest 10 frame gaps and counts >=500 ms, >=2 seconds, stages of list waits, GPU-completion logs, RAM headroom, and any texture/visual regression. Run an untraced A/B pass to quantify logging overhead.

**Caution:** the trace switches are read once at emulator initialization; launch a fresh process for a change to take effect. Do not clear shader caches or change game settings for this comparison.
