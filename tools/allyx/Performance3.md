# Ally X Performance 3

Built on the working native-event MotionFix2 source (local checkpoint 25a3760),
upstream Ally v6.2 dfd996c40ed4c26e75d42d7c61e7997402caa9de.

Shared engine changes, with no game-title checks or per-game configuration:

- Budget-aware recent-image retention, adapted from zortos293/Lichii PR 1435.
  For native tiling, retain the upstream driver-budget, allocation-pressure,
  fragmentation and host-commit bounds. Other games use their existing critical
  cache threshold as the retention limit. Do not subtract ages from early frame
  counters before enough frames exist. Diagnostic minimum ages cannot override
  emergency/aggressive collection (the interaction identified in PR 1311).
- Independent texture and buffer read-cache shortcuts from PR 1479. Preserve
  this fork's existing fast path and diagnostic disable switch. Publish GPU-write
  markers before read traps are armed. Buffer skipping retains the upstream
  Switch 2 capability guard; older games retain their required buffer checks.
- Mapping-aware Windows zero-page cleanup and five-minute maximum backoff from
  PR 1580 (original work by Frizti). The fast zero-page scanner was already in
  this fork and is unchanged.
- Memory-pressure-aware cold-page hints from PR 1581 (Julius Bairaktaris /
  ThatGuyMcd, based on Frizti's approach). Keep uploaded backing resident when
  RAM is available, resume cold hints below min(4 GiB, one quarter of host RAM).

Sources:
- https://github.com/zortos293/Lichii/pull/1435
- https://github.com/zortos293/Lichii/pull/1311
- https://github.com/zortos293/Lichii/pull/1479
- https://github.com/zortos293/Lichii/pull/1580
- https://github.com/zortos293/Lichii/pull/1581

The Eglantine nx2 branch was fetched and inspected. Its snapshot did not contain
the retention helper or the newer memory-pressure API; the specific merged
Lichii patches were adapted instead of merging a different emulator tree wholesale.

Validation: standalone motion checks and compile-time memory-policy boundary
checks. Full Release compilation/link and deployment are required before packaging.
No measured gameplay FPS improvement is claimed for this build. Benefits depend
on each game's workload. No game files, saves, keys or configuration are included.
Native Windows sensor-event code and Zen 4 compiler settings are preserved.
