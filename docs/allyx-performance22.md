# Performance22: format-zero compatibility and slow mesh build attribution

Based on Performance21 (0ab2fc8021). The 50-minute Ocarina session contains ten
texture-format assertions, all format zero: five with component types {0 0 0 0}
and five with {2 2 2 2}. Port upstream's explicit format-zero fallback from
Eglantine-Community/Eglantine nx2, snapshot e1b46769125213316265de67fdd8cfaf50f9fdc8
(format_lookup_table.cpp blob d05c1578dcf1db9d5ed9ffb0d5dd00dd3e729b8a).

This returns the same A8B8G8R8_UNORM fallback the previous code used after asserting.
Other unsupported formats still report assertions. This addresses the repeated
format-zero assertion path; it does not prove the descriptors are correct or that
texture corruption or a gameplay stall has been fixed. The ported file matches
the inspected upstream file exactly. Upstream is an unrelated snapshot in this
checkout, so the isolated change is ported instead of merging unrelated histories.

Pipeline 2de71abc97ca9e7e took 145.3 ms of GPU-thread preparation and then blocked
the Vulkan worker for 5026.779 ms. Mesh emulation creates mesh, optional task,
distribution, and raster pipelines. The existing timer combines those stages;
it cannot establish which stage was responsible. Slow-stage CPU-wall timings under
the existing wait-trace switch narrow the next diagnosis without changing shader
semantics, binary-cache keys or synchronization. These timings are not GPU execution
measurements and are not themselves a performance optimization.

Performance21's compiler priority, Performance20's guarded retention, native motion,
and the saved shader binary implementation are preserved. No shader-cache clearing
is required. Gameplay benefit from the format-zero handling remains unmeasured.
