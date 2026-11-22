# Lichii Ally X Performance18

Run lichii.exe directly. This diagnostic build preserves Performance17's AMD pipeline binary cache and native Windows motion.

The existing embedded EDEN_NX2_TEXTURE_WAIT_TRACE setting enables focused timing for slow texture uploads and scheduler waits. Texture timings distinguish memory preparation, reads, unswizzling, conversion and CPU-side upload recording. GPU-completion waits are CPU wall time waiting on the semaphore, not GPU timestamp measurements. Measurements can include OS scheduling and paging; phases must not be added to overlapping outer timings.

Only slow events are reported, with bounded logging. No extra GPU waits are inserted. This build is intended to identify the remaining bottleneck, not to promise an FPS improvement. Keep your usual settings and caches for a comparable session.

The existing missing-base-storage warning occurred during game-list scanning before Ocarina booted; it has not been attributed to Ocarina. No game files or saved settings were changed for this build.

After a normal session, ask to check the logs. Performance17 remains available in its separate folder.
