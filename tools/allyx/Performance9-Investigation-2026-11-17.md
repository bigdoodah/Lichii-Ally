# Performance9 GPU wait investigation — 17 November 2026

## Result

The longest recorded frame gap is localized to completion of a GPU-written command-list copy. It is not yet attributed to CPU command recording, the driver submission call, GPU execution, or residency/paging. No synchronization fix is justified solely by this timer.

Analyzed build: e8da7d5cc9e2ee03db2b87d5ce7fe5ed968c474f.
Capture: work/performance9-first-session.log (5,382,602 bytes).
Comparison: work/performance8-first-session.log. Routes and durations were not controlled.

## Evidence

| Measure | Performance8 | Performance9 |
| --- | ---: | ---: |
| Ten-second frame reports | 100 | 128 |
| Report windows containing a frame >= 1 second | 3 | 14 |
| Maximum recorded frame gap | 4.866 seconds | 9.420 seconds |
| Presents >100 ms in complete 600-present blocks | 63 / 29,400 | 253 / 34,800 |

These counts describe the sessions, not a controlled regression attributable to Performance9.

At 1403.274699 s, writer ce9f15d1fe2e4101 reported a maximum wait of 9,360,502 microseconds. The 1403.317123 s frame report recorded 9,419.7 ms with zero pipeline builds, zero translation time, and zero held-draw time in that worst frame. The front's nearby list GPU wait aggregate was about 9.39 seconds. The back was largely waiting for work.

The same writer appears in 246 P9 report buckets; six bucket maxima exceed 500 ms (989.9, 975.1, 9360.5, 793.8, 773.9, 1033.3 ms). It is absent from the P8 capture, which may reflect different gameplay. Another writer, 541ba22d44898196, has long waits in both captures: maxima 2.686 seconds in P8 and 2.701 seconds in P9. This type of wait predates P9.

Device-local allocated memory grew from 2577 to 6300 to 6701 MiB in preceding reports. Across the largest gap it was nearly flat, 6701 to 6680 MiB. Nearest free-RAM readings were 1.9 GiB before and 0.9 GiB afterward, not simultaneous measurements of the exact wait. Another long wait occurred with more than 3 GiB free. This keeps memory pressure plausible without proving it explains all stalls.

The cold-sweep counters report accumulated deletions, not deletion durations. They cannot attribute the stall to a sweep. No device-loss error was found in the captured emulator log. Access to Windows System events was denied, so absence of a driver reset has not been established from Windows events.

## What the timer actually measures

ReadGpuWrittenCommandList -> Nx2ReadListCopy -> WaitSubmittedTick -> MasterSemaphore::Wait.

Nx2ReadListCopy releases its list mutex before waiting. The wait targets the copy's timeline tick rather than a device-wide WaitIdle. The timeline-semaphore wait has no fixed nine-second sleep/retry delay; Vulkan errors throw.

Crucial distinction: Scheduler::IsSubmitted tests tick < CurrentTick. SubmitExecution advances that counter when handing work to the Vulkan worker, before that worker records commands and calls vkQueueSubmit. Therefore "0 unsubmitted" does not prove the driver already accepted the work when the wait began.

The duration can include worker backlog/recording, a blocked driver submission, preceding GPU work, residency delays, and the copy's own completion. The writer hash identifies which dispatch produced the copied commands. It is not a GPU stopwatch for that shader. "5 submissions behind" is a bucket average, not the precise backlog of the longest wait.

## Upstream comparison

- Lichii #1830, merged, f6c17d35f22d7a495fbd4b07f37bf802f23f302a:
  https://github.com/zortos293/Lichii/pull/1830
  Keeps qualifying GPU-written list words on the GPU and dispatches indirectly. Closest architectural match, but a large multi-file change. Applicability to this writer's consuming macros is unproven. Reported performance testing was MKW on PS5/RADV; OoT correctness checks covered intro/Deku Tree, not this Ally freeze.

- Lichii #2006, merged, af867ba09412aa6418fa92a5a8e09dab327c9ec8:
  https://github.com/zortos293/Lichii/pull/2006
  Defers/batches list-copy submissions. Our SubmitNx2ListCopies already implements this behavior.

- Lichii #1087, closed without merge:
  https://github.com/zortos293/Lichii/pull/1087
  AMD query availability fallback was dropped by its author after retesting found no settled-cache FPS benefit. Not selected.

- Lichii #1845, merged, 299df0ed5094477d0a161f487f0aedc8e85cb28a:
  https://github.com/zortos293/Lichii/pull/1845
  Earlier retirement of BadOverlap textures is a separate memory-pressure candidate. It does not establish the cause of the 9.36-second wait; retention/content-preservation implications still require review against this branch.

- Eglantine #825, merged:
  https://github.com/Eglantine-Community/Eglantine/pull/825
  Linux cold-page hints caused measured stalls. Our Windows path uses VirtualUnlock under memory pressure, so the Linux measurements do not prove the same cause here.

## Prepared next step

Trace-GPU-Wait.cmd runs the same Performance9 executable and adds the existing EDEN_DKB_FRAME_TRACE=1 counters to its normal diagnostic environment. Place it beside Performance9's lichii.exe; a copy is already there on this PC. Play normally and close normally. The regular Play-Performance9.cmd remains available.

This records CPU usage by role and GPU timestamp measurements for approximately one sampled frame per second. Look for DKB cpu, gpu_frame, gpu_submission, gpu_work, renderer, and frame records. Matching multi-second GPU measurements can identify expensive submitted work; worker CPU and submission fields can narrow CPU-side delays.

Limitations: sampling can miss a random stall, so missing long GPU measurements do not clear the GPU. The trace adds overhead and is not a benchmark. Arrays are bounded to 512 submissions and 4096 work records per sampled frame (roughly 400 KiB of CPU arrays, plus query storage). The trace uses inline counters/timestamps, not stack sampling.

Do not enable the timeline for a long random-stall capture: it clamps to 30 seconds and permits large per-thread buffers. The launcher clears timeline and stack-sampler flags. No debug-server capture, emulator start/stop, driver/settings change, or new executable build was performed during this investigation.

If sampled timestamps miss the event, the next code-level diagnostic is bounded submission-phase timing (worker handoff, queue-call entry/exit, completion) on slow waits. Preserve command completion ordering throughout; do not skip waits or fabricate completion values.
