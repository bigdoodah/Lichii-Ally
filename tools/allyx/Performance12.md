# Ally X Performance12

Start with `Play-Performance12-LowMemory.cmd` on the Ally X. It keeps the same compressed-texture setting as the Performance11 low-memory launcher. If you see grid lines on distant terrain, use `Play-Performance12.cmd`. Both launchers enable the motion and GPU wait changes for all games.

## Native motion

Performance12 tracks each emulated six-axis sensor handle separately. Previously, stopping an unused motion handle could also silence the active Pro Controller on the same player slot. Each handle now has its own start, stop and at-rest state. The Ally still uses the native Windows gyroscope and accelerometer through the existing `windows_motion` input mapping.

While playing Ocarina of Time, try tilting the Ally with the Bow or Hookshot drawn and the game's motion aiming enabled. The emulator log records handle requests and periodically reports the peak rotation speed at the controller input and in the samples delivered to the game. Those readings help locate where motion stops if aiming still fails.

## GPU query waits

Performance11's Ocarina of Time log showed substantial time spent waiting for GPU query results during several long frame gaps. Both Performance12 launchers enable the existing range-specific wait. When the game reads a counter, the renderer can continue once the results covering the requested memory range are ready. This reduces the opportunity for unrelated queued GPU work to delay the read. It follows [Lichii pull request 559](https://github.com/zortos293/Lichii/pull/559).

The emulator logs individual query reads that take at least 100 milliseconds. A `Slow query DMA read` line separates the time spent setting up and flushing the work from the time spent waiting for results. The `Renderer frame gap ended` timestamp marks the end of a long frame, and `Vulkan worker pipeline wait` identifies a worker stalled on a pipeline build. Shader compilation and driver stalls can also delay frames. Gameplay on this build is needed to measure the difference on the Ally.

You can use the same game settings and shader cache. To return to Performance11, run the original launcher from the Performance11 folder.
