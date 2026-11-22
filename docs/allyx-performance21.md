# Performance21: foreground pipeline compiler priority

Performance20 recorded compiler waits of 7.873 and 5.022 seconds overlapping major
renderer stalls. Scheduler GPU-completion timers include delayed submission and
are not measurements of GPU execution time. These logs identify blocking compiler
work but do not prove how much time Windows scheduling contributes.

On Windows integrated GPUs, the foreground VkPipelineBuilder pool now requests
Normal thread priority instead of Below Normal. CPU placement and worker count are
unchanged. This pool handles both boot preparation and demanded runtime pipelines,
including non-native titles. Separate background prebuild, warm-up, serialization
and precompile pools retain their policies. Other platforms and discrete GPUs retain
the old behavior. EDEN_ALLYX_PIPELINE_PRIORITY=0 restores the old policy.

The shared thread worker accepts an optional priority override; existing callers
retain their prior policy. A startup log identifies the selected compiler priority.
The policy test checks default, background, efficiency and overridden task contexts
and completion of queued jobs.

Upstream snapshot e1b46769125213316265de67fdd8cfaf50f9fdc8 retains the same low-priority
foreground pool. This is a local scheduling adjustment, not an upstream fix.

Native motion, shader binary storage, rendering output and Performance20 texture
retention are unchanged. There is no new shader-cache format or required cache wipe.
Normal compiler priority may reduce scheduling delay, but can increase CPU contention
while compiling. Driver compilation cost and gameplay improvement remain unmeasured.
