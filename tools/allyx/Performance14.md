# Ally X Performance14: reduce routine cache-cleanup work

Builds on Performance13 Direct (2b10f3bc13). Open lichii.exe in the complete package;
the same built-in low-memory profile, native Windows motion and diagnostic logging remain.

## Evidence

The saved November 18 Ocarina session recorded an 798.994 ms graphics pipeline
eviction-selection operation at 305.404 seconds, with no pipeline builds in that
805.2 ms frame gap. Texture collection also recorded individual operations over
500 ms. These are separate from the longer command-list waits.

22 of 26 suspended-list waits lasting over one second after queue submission
returned followed an explicitly logged terrain decode/upload within four seconds.
The front thread was still reporting during one such wait. This is evidence to
investigate texture/GPU workload, not proof of a lost wake-up or driver defect.

## Changes

- Count built graphics pipelines and collect eligible candidates in one traversal.
  Select using compact last-use/key-reference records. Copy only the chosen keys
  into the persistent removal queue. References never survive the function or a
  cache mutation. Keep the same oldest-first selection, age, resident budget,
  prebuilt protection, deferred removal and GPU-fence retirement checks.
- Give routine texture collection a shared soft deadline: 2 ms below expected
  usage, 4 ms above expected but below critical. Always permit one eligible image
  to progress. Old-image, normal and aggressive passes share the budget. Keep
  collection unrestricted at critical usage or after an allocation failure, so
  memory recovery cannot be throttled by this change.
- Preserve image eligibility, downloads, GPU completion waits, guest writeback,
  tracking and deletion ordering. A driver call, page fault, download, or scan can
  still exceed the deadline. This is not a strict frame-time ceiling.

Less routine collection can retain images for extra frames. Critical recovery is
unchanged. Shader selection still scans the cache; it is cheaper but not split
across frames. No changes to synchronization, input, shader output, saves, settings
or on-disk shader caches. No 60 FPS or freeze-elimination claim.

## Upstream review

Fetched Eglantine-Community/Eglantine nx2 and examined its selection implementation;
its imported history is separate from this checkout, so no unrelated-history merge.
Reviewed Lichii PR 652 (compact candidate selection, but different removal/policy),
1306 (collection must keep up under critical pressure), and 2167 (host-spill
collection correction; the local tree has no corresponding spill-collection path).
The compact-reference idea follows 652 without replacing this build's policies.
The routine soft deadline is local code. PR 752/933 yielding/polling is already
incorporated. PR 2078 targets list-copy allocation overhead, not a proven cause
of this session's terrain-associated stalls; it is not bundled here.

https://github.com/zortos293/Lichii/pull/652
https://github.com/zortos293/Lichii/pull/1306
https://github.com/zortos293/Lichii/pull/2167
https://github.com/zortos293/Lichii/pull/752
https://github.com/zortos293/Lichii/pull/933
https://github.com/zortos293/Lichii/pull/2078

## Validation

Test-Performance14.py compiles the actual previous/current selection functions
against controlled cache contents and compares selections across eight cases:
ordinary, pending builds, protected prebuilt pipelines, recently used, null
entries, preload-all, MoltenVK and age cutoff. It counts key copies and checks
that selection does not remove objects. Budget checks cover first-image progress,
deadlines and unrestricted emergency recovery. Package verification records the
build and unchanged motion/input/startup sources and native motion object.

These checks do not measure gameplay. Use normal play; the existing log will show
whether cleanup hitches fall and whether memory stays controlled. Keep the
Performance13 Direct folder as rollback. The unresolved long terrain-associated
GPU waits may remain.
