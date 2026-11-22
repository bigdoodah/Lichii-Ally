# Lichii Ally X Performance17

Open lichii.exe in this folder. No CMD launcher is needed. Performance16 remains available separately.

This build adds a dedicated Vulkan pipeline-binary store for supported AMD Windows drivers. It is intended to reuse finished driver pipelines across launches and after an in-memory eviction, reducing repeated compilation stalls. First-use compilation can still occur; this is not a promise of 60 FPS or stutter-free gameplay.

The implementation is adapted from the upstream standalone pipeline-binary cache, with the AMD enablement and retention behavior described in Lichii PRs #1406 and #1407. It preserves this fork's existing shader translation cache, Performance15 precompiled fast path, Performance16 Mario Kart timing and shared overhead changes, and native Windows motion.

Use Vulkan and leave Use Vulkan pipeline cache enabled. The new store requires the pipeline-binary and maintenance5 features and checks the driver's cache preference. Unsupported configurations retain the existing pipeline creation path.

The stored data is specific to the device and driver. A first run may populate the new store. Close the game normally, then revisit the same area after restarting; the cache diagnostics should show stored pipelines being restored. Existing shader caches are not deleted.

Validation and exact build identity are recorded in BUILD-VERIFICATION.txt. The first gameplay comparison on the Ally remains necessary to establish whether the earlier 4-6 second hitches are reduced.

Upstream references:
- https://github.com/zortos293/Lichii/tree/79e43b0acbcf60ba7eb5658a85987125ec82eeff
- https://github.com/zortos293/Lichii/pull/1406
- https://github.com/zortos293/Lichii/pull/1407
