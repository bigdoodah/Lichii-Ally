# Performance19: large ASTC texture lifetime attribution

Performance18 logged 19 slow uploads of 1365x1365, 11-mip, 90-layer arrays
with flags 0x1007 -> 0x1013. Neither contains Registered (0x40): these are
uploads during new host-image creation, not ordinary registered-image updates.
Several uploads precede long scheduler completion waits. This correlation does
not measure the GPU execution time of the conversion itself.

Performance19 records why large ASTC arrays leave the cache: garbage collection
(normal, aggressive, or allocation failure), cold sweep, CPU unmap, ignored
overlap, joined copy, or relocation. GPU unmaps, creation and registration are
also recorded. Each event includes addresses, slot ID, allocation frame, last-use
age, flags, shape, estimated image memory and current cache thresholds.

The trace uses the existing embedded EDEN_NX2_TEXTURE_WAIT_TRACE=1 setting.
It covers layered ASTC images of at least 32 MiB guest size, capped at 2048 events
per texture cache, with an explicit limit message. Slot IDs and addresses can be
reused; correlate with the allocation frame and ordered creation/deletion events.
LRU age is read before unregistering frees the entry; unregistered events report
registered=false and an inapplicable idle age of zero. Memory values are cache
accounting/estimates, not proof of physical memory residency or hard paging.

No cache budget, eviction policy, synchronization, conversion, texture output,
shader cache or input/motion code changes. No speedup is claimed for this build.
The next behavioral change depends on whether the trace identifies avoidable
eviction or required guest-memory remapping/replacement. Address-only converted
texture reuse would be incorrect if contents changed.

Upstream review on 2026-11-18: Eglantine PR 900 changes texture-handle indexing;
PR 909 includes render-target allocation/descriptor reuse and broader CPU-path
optimizations. Their texture diffs do not establish the deletion cause of these
layered ASTC arrays. Lichii issue 1035 reports repeated arrays of the same shape
under memory exhaustion, but is not evidence that this session has that cause.
The fetched upstream nx2 snapshot has unrelated history; no forced merge.

Validation: compile the Windows x64 executable; inspect all unregister callsites;
verify native motion/HID and pipeline-binary-cache sources against Performance17;
verify the native motion object hash, executable build/trace markers, unchanged
runtime imports and packaged DLL/archive integrity. Gameplay validation remains
for the owner's next normal session; do not start or stop their emulator.
