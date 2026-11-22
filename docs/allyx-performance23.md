# Performance23: actual ASTC scratch allocation budget

Temporary ASTC decode images were retained against a 256 MiB budget using logical texel sizes. Vulkan tiling and alignment can make their allocations larger. The main texture cache already uses actual allocations; this change corrects the separate scratch pool.

- Store VMA allocation bytes for each decode scratch image; completed-image retention uses actual bytes and rejects unknown sizes.
- On integrated GPUs with aligned layer offsets, reduce a padded allocation exceeding 256 MiB before recording work. At least one layer is retained.
- Reuse a smaller completed batch of the same dimensions/mips before allocating, avoiding repeated oversized probes.
- No changes to encoded texture format, shader cache, native motion, synchronization, or final texture retention policy.

Limits: the initial allocation probe can exceed the target briefly; the target is per decode image and for the idle pool, not all in-flight allocations. Output staging and final textures still consume memory. This corrects accounting; gameplay improvement and the cause of the full 2.8-second hitch remain unverified.

Evidence: P22 Ocarina snapshot review-eden_log.txt.old.txt shows a 102.24 ms upload at195.433s, 2799.7 ms renderer interval ending197.848s, zero GPU-thread pipeline builds/draw holds, and0.6GiB free RAM at189.762s. These timestamps do not isolate GPU decode time.

Upstream review: local upstream/nx2 e1b46769125213316265de67fdd8cfaf50f9fdc8 has the same logical scratch estimate. Live file retrieval was unavailable; targeted GitHub PR search did not identify a scratch-allocation correction. No broad upstream merge attempted (unrelated snapshot histories).

Validation: six existing/extended transcode-reaper cases compiled with clang-cl C++20 and ran through a local assertion adapter (not the full Catch2 suite). Covers completed-prefix refresh, no unnecessary refresh, unfinished resource preservation, budget admission, same-frame reuse, padded allocations, and unknown allocation sizes. Windows application build and packaging verification recorded separately. No game was started or stopped for testing.
