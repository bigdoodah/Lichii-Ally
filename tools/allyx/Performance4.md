# Ally X Performance4

Builds on Performance3 (f0f3edd) and preserves the event-driven native Windows motion driver.

## Changes

- Adapted https://github.com/zortos293/Lichii/pull/1531 : x86-64 Dynarmic register allocation, smaller associative dispatch table, deeper return stack, compact exits, hot/cold layout, redundant zero-extension removal, address folding, host flags, and host call/return. Paranoid accuracy keeps defaults; cycle-counted execution keeps its required checks. Our older allocator uses construction instead of the upstream Reset/dirty-register mechanism; options are passed through its constructor.
- Adapted https://github.com/zortos293/Lichii/pull/1373 : hardware interrupt-flag waiting (AMD MONITORX/MWAITX, Intel WAITPKG when supported), adaptive poll window, yield fallback. Existing core-lock and explicit idle-pause debug paths remain. Upstream optional statistics collection was omitted.
- Inspired by the bound-write portion of https://github.com/zortos293/Lichii/pull/1441 : single-page cached GPU-to-device/host translations for cached constant-buffer writes. Cache keys include manager identity, mapping generation and device backing generation. Sparse writes, cross-page writes and syncpoint shim ranges take the old path. Pending read downloads finish before writes; original invalidation accumulation remains.

## Scope and limits

PR #1441 is NOT fully ported. This baseline has no Nx2FastDraw delta framework, uniform-keep state, or the newer buffer-cache ports on which cross-draw uniform reuse depends. Importing just that reuse would miss invalidations. The compatible page-write optimization avoids a wholesale renderer replacement.

Changes affect shared emulation paths, not game-specific profiles. Hardware idle polling defaults to Switch 2 workloads as upstream; the ordinary blocking path remains for other titles unless explicitly configured.

## Verification

The focused JIT checks are standalone adaptations of upstream random-block equivalence (1,000 blocks) and five host-call regressions (nested calls, unusual returns, deep recursion, FPCR/indirect calls, register spills). Idle tests cover timeout, expired deadline, wake on store and poll-window bounds. Existing native-motion and texture-memory policy checks remain.

The upstream fastmem fault regression is also adapted to Windows VirtualAlloc/VirtualProtect to exercise fault fallback and the subsequent recompiled block. A buffer-write test checks GPU remapping, device-backing remapping, page boundaries, out-of-range addresses and unmapping. No gameplay FPS improvement is claimed without a game comparison.

Performance3's executable and package remain available for rollback.
