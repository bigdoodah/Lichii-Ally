# Ally X Performance5

Builds on Performance4; native Windows motion and previous CPU/memory changes remain.

- Spread destruction of evicted graphics pipelines over frames: at most 32 objects or a 1 ms soft deadline per frame. Under host memory pressure, allow 128 objects or 2 ms to release memory faster. GPU completion fences still gate every destruction. An individual driver call can exceed the deadline; this is not a hard frame-time guarantee.
- Adapt the resident built-pipeline count from https://github.com/zortos293/Lichii/pull/1334 . Failed/null and pending cache entries no longer cause additional built pipelines to be evicted. Keep the existing conservative budget and prebuilt retention policy rather than importing the larger upstream quarter-RAM budget.

The owner's Performance4 log showed a 2,397-pipeline eviction batch and low available RAM. These changes target unnecessary eviction and concentrated destruction work; the log does not prove that these caused every hitch. Some worst frames had no shader-build wait. FPS gains have not been measured.

Upstream Eglantine nx2 cf6cfe9 was inspected; its destruction loop is also unbounded. Lichii PR #408 attribute-key deduplication is already present in this baseline. No cache format or shader output changes, no cache deletion, no settings changes, no new draw skipping. Existing asynchronous compilation behavior is preserved.

The bounded retirement policy is a local change. It can retain evicted objects for additional frames, trading short-lived memory retention for less concentrated cleanup. Memory-pressure mode accelerates draining. Shutdown still destroys all remaining objects normally.

Validation: focused regression covers unfinished GPU fences, per-frame limits, elapsed-time limits, forward progress after a slow driver call, complete draining of a 2,400-entry backlog, and eviction bounds. Release build/link and package verification are recorded separately. No live gameplay benchmark was run; the debug server was not listening.

Keep Lichii-AllyX-Performance4 as the rollback folder.
