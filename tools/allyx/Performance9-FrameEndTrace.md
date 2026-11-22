# Performance9 FrameEndTrace

## What the new capture established

The traced Performance9 session contains several different stall paths. At renderer frames 5898 and 6738, frame-end processing took 2688.894 and 2703.326 ms. The GPU-command thread consumed almost the full corresponding wall interval as CPU time. The DKB trace-reading code, early memory-owner sample, and (for the first gap) precompiled-pipeline and translation reporting had finished before these gaps.

High CPU time does not prove expensive cleanup: texture and buffer caches use spin locks, so the command thread could be spinning while another thread holds a cache lock. This build measures lock acquisition separately from the cache body.

Other events remain distinct: a 2687.949 ms command-list completion wait, a guest-CPU-heavy approximately 4.1-second interval, and one sampled GPU frame with a 2327.138 ms timestamp span. That GPU frame's 608 timed game draws/dispatches totaled only 6.351 ms; its long submission intervals include work and dependencies outside those game-shader timings. Timestamp spans are not GPU utilization measurements.

## Diagnostic change

EDEN_NX2_FRAME_END_TRACE=1 reports individual frame-end sites taking at least 100 ms. It measures pipeline/descriptors/fence/staging maintenance, texture and buffer lock waits separately from their work, and selected texture-cache internals including completed-image retirement, garbage collection, synchronous download/Finish/CPU swizzle, deferred destruction, decode work and runtime maintenance.

The cleanup order, lock lifetimes, eviction policy, completion waits and stored texture data remain unchanged. Disabled tracing does not read the clock. Enabled scopes use fixed stack state; only slow calls produce a log entry. This is a diagnostic build, not a claimed performance fix.

A particularly relevant candidate is TextureCache::RunGarbageCollector: it can synchronously download and CPU-swizzle multiple GPU-written images on one frame. The existing EXP57 limiter is off; enabling it is deferred until the cost is attributed. Low free RAM around the two CPU-active stalls (approximately 0.5–0.6 GiB) is context, not proof of the cause.

Upstream Lichii #1039 provides precedent for timed memory/cache sites, but this fork lacks its broader tracer infrastructure:
https://github.com/zortos293/Lichii/pull/1039
The local diagnostic specifically separates spin-lock acquisition and deferred destruction, without importing rendering changes.

## Native motion audit

The native motion implementation is byte-identical to the files in the working Lichii-AllyX-Native-v6.2-MotionFix2 source patch and to baseline 25a37602903cf592116bee95de1bef91c0210cc2. The native .cpp/.h/events/math files, input registration, input build configuration, input UI, and HID motion handling were unchanged through Performance9. This diagnostic does not modify those files.

The installed Performance9 executable before this diagnostic matched its delivered SHA256:
188ccdbf3637e2f99eff769659a88a6cbc5e5e8edc81a14f1b9c2c7fb058ab1c

Current motion is enabled. Connected player 0 uses windows_motion, with no selected input-profile override; OoT uses the global motion setting. P8, P9 and traced P9 all report native sensor event delivery at a requested 10 ms interval, motion fix 2. Axis mapping, units, and the 100 ms stale-angular-velocity reset remain unchanged.

The old ally_x_native engine warning belongs to disconnected player 7. That unused mapping was not edited. Successful initialization and unchanged source do not measure end-to-end input latency during a stalled game frame.

## Running

Run Play-FrameEndTrace.cmd in this diagnostic folder for the next normal session. It keeps async readback and existing GPU/list-wait counters, adds slow frame-end timing, and disables sampled DKB GPU queries, timeline tracing, stack sampling and the debug server. Existing settings, saves and shaders are used. The original Performance9 folder and launchers remain available.

No emulator launch/stop or gameplay benchmark was performed by the assistant. Build and package checks are recorded separately in BUILD-VERIFICATION.txt.
