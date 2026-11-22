# Lichii Ally v6.2: Ally X custom build checkpoint

Base: Syyrhc/Lichii-Ally, tag ally-v6.2, upstream commit dfd996c40ed4c26e75d42d7c61e7997402caa9de.

## Changes

- Optional Windows native motion backend using C++/WinRT Gyrometer and Accelerometer, ported from the earlier source kit.
- Input settings expose Built-in Windows motion (ROG Ally X). SDL remains available. Selecting the motion-only device preserves button and stick bindings.
- Native readings use a 10 ms requested interval (respecting hardware minimum), unit/coordinate conversion, stale-reading handling, retry and shutdown cleanup.
- Optional Windows x64 Clang Zen 4 compiler target (LICHII_ALLYX_Z1E). The build helper uses upstream Windows PGO data and ThinLTO, with two compile/link workers by default.
- PowerShell 5.1 compatible build helper and standalone motion checks.

## Verified on this Ally X, November 16, 2026

- LLVM 22.1.8 x64 portable compiler executes; official archive SHA256 verified.
- Qt 6.8.3 MSVC x64 and Qt Charts installed locally. A compiled smoke program links and runs: Qt 6.8.3: Charts linked; points=1.
- Native C++/WinRT sensor diagnostic compiled using LLVM 22.1.8: 100 gyro updates, 100 accelerometer updates, 100/100 finite samples, requested interval 10 ms.
- Coordinate, unit, timing and stale-reading regression checks pass in Release.
- Main CMake configuration recognizes the Zen 4 target, but cannot fetch OpenSSL's dependency checksum through this session's restricted network. The complete emulator has NOT compiled or linked yet.

No custom emulator executable or measured FPS improvement is included. Full application integration, motion directions in a game, stability and frame pacing remain unverified. The existing official app configuration has not been switched to the new backend.

## Resume on this computer

Double-click the existing Build-Lichii-AllyX.cmd in:
C:\Users\eloi3\Documents\Codex\2026-11-16\new-chat

It uses LLVM, Qt and source already extracted under that folder's work directory. Keep that directory together. The launcher requires normal internet access to fetch the upstream dependencies. Further compilation errors, if any, will be recorded in work\v62-build\allyx-build-*.log.

Only after a successful build, the custom app will be at work\v62-build\bin\lichii.exe. Keep the whole bin folder, including DLLs and plugins. In the custom app's input settings, select Built-in Windows motion (ROG Ally X), or use the motion binding's context menu. This option is not available in the existing official binary.

## Restore source changes elsewhere

This archive is a source checkpoint, not an application or self-contained toolchain. Apply Lichii-v6.2-AllyX-native-motion.patch to the exact upstream tag with git apply. Install LLVM 22+, MSVC/Windows SDK, Ninja/CMake, Python, Vulkan SDK and an MSVC x64 Qt kit including Charts; then run tools\allyx\Build-AllyX.ps1 -QtRoot <Qt directory>.
