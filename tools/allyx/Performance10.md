# Ally X Performance10: bounded pipeline-cache removal

Builds on FrameEndTrace (2ee74721beca372e421064ee166eb0b831488124) and retains the previous Ally X changes and native MotionFix2.

## Evidence and scope

The first FrameEndTrace session recorded 782.996 ms of pipeline maintenance immediately around removal of 1,802 graphics pipelines, leaving 3,321 entries. Performance5 bounded the later destruction of those objects, but candidate removal and transition clearing still happened in one batch.

This change targets that cache-removal hitch. It does not claim to fix the separate 5.62-second pipeline-readiness wait, command-list completion waits, or the 805 ms texture cleanup in the same session. Those timings overlap in some frame windows and must not be added together. Precompiled shader data was being used successfully; driver pipeline creation can still take place during gameplay.

## Change

- Keep candidate selection at the existing 600-frame interval, the 9,000-frame age cutoff, RAM-scaled resident budget, and protection for up to 2,000 unused prebuilt pipelines.
- Remove selected candidates over subsequent frames: inspect at most 32 candidates with a 1 ms soft deadline, or 128 with 2 ms when the existing host-memory-pressure detector is active. Reused, missing, unbuilt and currently bound candidates are skipped. Skipped candidates count toward the limit too.
- Invalidate cached transition pointers with a generation checked by both lookup paths and by insertion. This avoids walking every surviving pipeline after each small removal batch. Stale transition lists are cleared before they can return a pointer.
- Preserve mapped storage-pointer metadata before moving each removed object into the existing retirement queue. Each batch uses its current scheduler tick; Performance5's GPU-fence checks and destruction budgets remain in force.
- Keep the existing explicit invalidation after a failed precompiled pipeline. Do not change shader output, draw skipping, cache format, cache files, saves or persistent settings.
- Add separate opt-in slow-operation timers for candidate selection and removal. The normal launcher retains the lightweight FrameEndTrace logging to make any remaining expensive selection or single removal visible.

Candidate selection still scans the cache and chooses its oldest eligible entries in one pass. A single allocation, page fault or driver call cannot be interrupted. These are soft budgets, not a guaranteed frame-time ceiling. Delaying removal retains some objects for additional frames; low-memory mode drains more quickly.

## Upstream review

No equivalent bounded removal implementation was found in the reviewed changes. Lichii #652 still removes its selected set and clears survivor transitions in one pass; current #408 no longer changes eviction. Eglantine #897 confirms the need for deferred GPU-tick retirement and transition invalidation. Performance5's resident-count correction from Lichii #1334 remains intact.

- https://github.com/zortos293/Lichii/pull/652
- https://github.com/zortos293/Lichii/pull/408
- https://github.com/zortos293/Lichii/pull/1334
- https://github.com/Eglantine-Community/Eglantine/pull/897

The local generation invalidation and removal budget are new code, not an upstream fix claimed to be proven on this device.

## Native motion and validation

No input or motion code is changed. Verify the native files against MotionFix2 baseline 25a37602903cf592116bee95de1bef91c0210cc2 and compare the compiled windows_motion object before and after the build. The previous native audit is included with the package.

Focused checks exercise hard candidate limits, soft deadlines, skipped-candidate accounting, progress after one slow operation, complete draining of 1,802 candidates, and transition lookup/add after object removal and generation changes. The existing retirement regression checks GPU-fence safety. Build and package results are recorded in BUILD-VERIFICATION.txt.

No emulator is started or stopped by the assistant. No live gameplay improvement is claimed without a run of this executable. Keep the existing FrameEndTrace and Performance9 folders as rollback copies.

Run Play-Performance10.cmd in the complete Performance10 folder. Existing settings, saves and precompiled shader data carry over.
