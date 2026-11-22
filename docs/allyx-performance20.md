# Performance20: guarded ASTC retention and allocation accounting

P19 (58a003b596) recorded repeated aggressive GC of the same 1365x1365x90 ASTC
arrays at 11 idle frames. At 273.192 s both were removed, recreated at 278.688 s,
and followed by a 4141 ms frame gap with no shader builds. This proves eviction
and recreation, not that all GPU waits have the same cause.

Changes:

- Vulkan non-sparse ASTC accounting uses the original image's VMA allocation
  size when nonzero, including dedicated allocations. The former global ASTC
  estimate ignores per-image opaque BC1 selection and format/mip overrides.
  Sparse/unbacked accounting, scaled-image accounting and other backends retain
  their existing paths. The exact numerical error in P19 was not measured.
- Cleanup rechecks the existing eviction age for each candidate. Earlier
  deletions can restore headroom after an aggressive pass selected 10-frame
  candidates; a pass no longer keeps that short age for all remaining images.
- On integrated GPUs using native tiling, large layered costly ASTC images may
  remain cached until 1800 idle frames. This applies only with at least 512 MiB
  device headroom beyond allocator slack, 2 GiB system commit headroom and 1 GiB
  available physical memory. Unknown values, an allocation failure or recent
  allocator pressure disable this extra retention. Remapped, bad-overlap,
  CPU-modified and unbacked images do not receive it. Memory queries occur once
  per collector pass, not per candidate. Existing thresholds are unchanged.

Usage is the backend's all-heap allocated-usage metric, not texture-only memory.
The device budget is the backend's integrated-GPU-capped budget, not the entire
physical RAM or uncapped hardware heap. Retention never changes guest-content
invalidation, unmapping, barriers or image-conversion output.

P19 lifetime tracing stays enabled; `ASTC retention:` summarizes protected
candidates and the current headroom guard every 600 frames when GC runs.
Native motion, HID, pipeline binary cache and game settings remain unchanged.

Validation: standalone C++ assertions cover device/slack/commit/physical
boundaries, unknown values, allocation pressure, overflow-sized inputs and
rechecking eviction age after pressure falls. Build Windows x64 and verify
runtime/package integrity and unchanged input/cache sources. Actual hitch
reduction must be measured in normal gameplay; no FPS target is promised.

Upstream texture changes reviewed for P19 (Eglantine 900/909) do not supply these
allocation-accounting/per-candidate-age corrections. No unrelated-history merge
or unrelated performance bundle was applied.
