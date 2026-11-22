# Performance7 — syncpoint notification candidate

Built on Performance6; native Windows motion and previous Z1/CPU/memory changes remain.

The graphics service previously notified the guest only when the armed-bit acknowledgement changed. An armed syncpoint whose value advances now also triggers notification, even if the armed bit remains unchanged. This does not fabricate counter values, satisfy unfinished GPU fences, or change polling frequency.

Adapted from Lichii PR #2052 (head 2ad096d777dca8624421eed342fd5848c2f3439b), originating from joebenberry-afk/Eglantine commit 3216779ea3bb2cbfaf568822211f44e6a65fed9c and Orchid 7f848ae by MythosOfs, GPL-3.0. This base polls values on the timing thread rather than using upstream's immediate-mirroring guard; the separate poll snapshot is retained.

https://github.com/zortos293/Lichii/pull/2052

The regression harness extracts the actual production UpdateSyncptValues and PollSyncpts methods. Against fake memory, syncpoint manager and events, it checks unchanged armed bits, IDs across word boundaries, counter wraparound, already-mirrored updates, unarmed changes, normal acknowledgements and missing memory. The pre-fix version must fail and the patched version must pass. This does not test OS scheduling or reproduce the user's freeze.

Upstream's proposal is not yet merged and did not reproduce the original gameplay freeze. This is a focused correctness candidate, not a proven cure or FPS guarantee.

Run Play-Performance7.cmd for the same async-readback mode as the latest Performance6 trial plus timing counters. Run lichii.exe for normal defaults without that override. Performance6 remains untouched as rollback. No sampler or debug server is enabled.
