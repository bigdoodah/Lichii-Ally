# Ally X Performance13

Run `Play-Performance13-LowMemory.cmd`. If distant terrain has grid lines, use
`Play-Performance13.cmd`. Both launchers include the Performance12 native motion
correction and GPU query-wait changes.

Performance13 makes completed temporary texture-decode images available for reuse
during the same frame. A burst of texture loading can reuse or release scratch
memory earlier, reducing unnecessary allocations and memory pressure. An image
becomes reusable after its final GPU work has completed.

The change adapts the completed-scratch handling from
[Lichii pull request 1816](https://github.com/zortos293/Lichii/pull/1816),
head `2d8a5fed33cc1b7be9a40b909a64f7682f768c01`. That proposal is open upstream.
This build uses the existing 256 MiB scratch-cache limit, which accommodates the
approximately 218 MiB decode batches used for Ocarina's large terrain arrays.

The Performance12 log recorded a 3.33-second frame gap near terrain loading.
The surrounding ten-second profile included about 2.07 seconds of texture upload
work, and a nearby measurement showed about 300 MiB of free RAM. Those readings
support investigating texture loading. They do not identify the entire pause's
cause. Some unmapped reads can be expected for sparse, unwritten memory.

Both launchers enable `EDEN_NX2_TEXTURE_WAIT_TRACE=1`. The log identifies
synchronous texture uploads taking at least 100 ms, with their addresses,
dimensions, frame, flags, staging time and page-fault count. These records remain
available throughout the session.

Five focused checks passed for completion tracking, cache limits, unfinished GPU
work and the actual terrain-batch dimensions. The Release app and package are
verified separately. Gameplay FPS and pause reduction require a run of this build.

Use your existing settings and prepared shaders. Let an active shader-preparation
run finish before comparing ordinary play in a familiar area.
