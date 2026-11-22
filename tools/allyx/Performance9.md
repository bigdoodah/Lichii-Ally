# Performance9 - Windows memory-protection lock fix

Built on Performance8. Native Windows sensor-event motion, Zen 4 compiler settings, texture prefaulting, and previous CPU/memory/syncpoint changes remain.

Windows HostMemory::Protect now first attempts VirtualProtect on the requested range without taking placeholder_mutex. When Windows refuses a range across views or unmapped gaps, it falls back to the existing locked per-view walk. This avoids making ordinary single-view write-trap updates wait behind unrelated mapping work. Map/unmap locking and the requested protection flags are unchanged.

Exact production port of Reiixan's merged Lichii PR #1780, head 67adf25b9dc404867556e2d3f6c5f07550b2cd7a:
https://github.com/zortos293/Lichii/pull/1780

The upstream Windows test is also included: VirtualQuery checks read-only/no-access/read-write permissions inside one view, across two views, and over an unmapped gap; a final write verifies the backing alias. The focused local harness and build results are recorded in BUILD-VERIFICATION.txt. Passing these checks validates the exercised permissions and mapping cases, not every possible concurrent mapping schedule.

Reason for the change: Performance8's late 4.34-second frame gap had no pipeline compilation or held draws in that frame. The surrounding ten-second profile attributed about 3.2 seconds to FillImageViews. Overlap replacement inside that region calls UntrackImage, cached-page updates, and ultimately HostMemory::Protect, outside the nested TextureUpload timer. The log does not separately time this mutex, so the fix is a candidate for the pause rather than a proven cause or cure. Upstream measured reduced lock waits on another Windows PC; no Ally FPS gain is claimed.

The same window had little free RAM and higher private residency/peak commit. Driver allocation, paging, and texture retention remain possible contributors. PR #1845 (obsolete overlapping-image retention) and ASTC decode batching are separate follow-ups, not part of this build. No cache-budget or user-setting change is included.

Run Play-Performance9.cmd after closing the old emulator normally. It retains Performance8's async-readback trial and lightweight timing counters. Running lichii.exe directly uses the normal defaults. Performance8 remains the rollback folder. No sampler or debug server is enabled. Gameplay verification is still required.
