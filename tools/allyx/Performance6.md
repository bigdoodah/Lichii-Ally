# Ally X Performance6

Built on Performance5, preserving native Windows motion and all previous optimizations.

## Changes

- Zero-page cleanup now respects its measured cost. A productive 1.47-second scan formerly reset its interval to 5 seconds; it now waits 60 seconds. Fast productive scans retain the 5-second interval. Low-yield scans retain the existing exponential backoff to 300 seconds. The cost-based cooldown targets 1% scan duty up to a 60-second cap; it is not a hard CPU-time or frame-time guarantee.
- After ten busy mapping windows, skip the scan rather than starting it during continued mapping activity. The worker remains stop-token aware and retries after its current interval.
- Adapt the allocation-failure safeguard from https://github.com/zortos293/Lichii/pull/1353 : optional cleanup catches only std::bad_alloc and backs off 60 seconds without allocating a log message. Other exceptions retain their original behavior.

The fresh Eglantine nx2 device_memory.cpp has no equivalent cleanup worker. PR #1580's earlier backoff and mapping checks were already included in Performance3; this adjusts their scheduling using the observed scan cost.

## Evidence and limits

Performance5 recorded random 3.24- and 3.26-second frames with no shader waits, around 0.8 GiB free RAM, and separate 1.46-1.47-second cleanup passes. These establish memory pressure and expensive cleanup, not proof that cleanup accounts for every stall. This patch reduces avoidable scan frequency; it does not cap an individual scan or repair the unconfirmed shutdown hang. Delaying scans may leave zero-filled pages resident longer, and continuous mapping can defer them further. No buffers, page contents, shader caches, game settings or motion bindings are discarded or changed.

The in-process stack sampler was not used again after the diagnostic-session closure. No gameplay performance gain is claimed until observed.

Focused tests cover cooldown for the logged durations, low-yield backoff, recovery for cheap productive scans, successful results, injected allocation failure and propagation of unrelated exceptions. Release build and package results are recorded in BUILD-VERIFICATION.txt. Keep Performance5 as rollback.
