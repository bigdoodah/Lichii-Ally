# Performance8 - Windows texture-upload prefault candidate

Built on Performance7; native Windows motion, the syncpoint correction, and earlier Z1/CPU/memory changes remain.

Before a whole CPU texture upload of at least 1 MiB, read one byte from each mapped 4 KiB guest page using the calling thread and up to two idle texture workers. This distributes Windows soft-page-fault work before copying/untiling. Large chunked uploads prefault only the current 32 MiB chunk. Trimmed-mip, sparse-tile, and layer-only paths keep their existing behavior. No page is locked in RAM, and cache budgets and cold hints are unchanged.

Adapted from zortos293's Lichii PR #2043, head 2497e75400b9dfbbdec3114ea8290ba47004d0f2:
https://github.com/zortos293/Lichii/pull/2043

The proposal is open/unmerged. Its author measured improved Windows dual-view memory throughput in an isolated upload benchmark, not in-game improvement. The local cap is two helpers rather than seven; three participating threads follows the reported scaling plateau and limits contention with the Ally's guest CPU threads. Linux remains a no-op. The separately fetched Eglantine nx2 tip was cf6cfe9; this imported source kit has no shared Git ancestry, so this is a focused port rather than an unrelated-history merge.

Why this candidate: the latest Performance7 log contains 5.58 s and 3.33 s frame gaps with little pipeline compilation or async-readback wait cost. Near the 3.33 s gap, aggregate texture-upload time rose to about 1.89 s over a 10 s window and allocator device-local allocation grew by about 1.94 GiB, while free system memory was about 0.4-0.9 GiB. This is evidence to investigate texture/memory work, not proof that soft faults caused the full stall. Prefaulting cannot fix VRAM allocation growth, hard paging, or an unrelated synchronization stall.

The existing ten-second GPU profile now separates `texture prefault` from `texture upload`. Their sum is the comparable upload cost: zones record exclusive time. This avoids per-texture log spam and stack sampling. The launcher keeps Performance7's async-readback trial and lightweight counters; no user settings are rewritten.

Run Play-Performance8.cmd after closing the previous emulator normally. Keep Performance7 for rollback. Compare ordinary gameplay in the same saved area; no lengthy test matrix is required. Build/regression validation is recorded in BUILD-VERIFICATION.txt. Actual freeze reduction and FPS improvement remain unverified until gameplay.
