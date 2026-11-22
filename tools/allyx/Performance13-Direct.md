# Ally X Performance13 Direct

Double-click `lichii.exe`. The ten options enabled by
`Play-Performance13-LowMemory.cmd` are now compiled into the Ally X executable's
startup defaults. Keep the DLLs, plugins and other support files beside the app.

The options enable compressed texture storage, asynchronous GPU readback,
cooperative command-list processing, query handling, and the existing timing and
motion diagnostics. This is a convenience update to Performance13, with the same
renderer and native motion code. No additional frame-rate gain is claimed.

The original launchers remain available. `Play-Performance13.cmd` overrides the
low-memory texture option to off, useful if compressed terrain shows grid lines.
`Play-Performance13-LowMemory.cmd` explicitly selects the built-in profile.

Explicit environment variables take priority over switch files. The user's
`nx2_switches.txt` takes priority over the copy beside the executable, and both
take priority over the compiled defaults. This preserves deliberate overrides.
The original CMD files also clear inherited sampling, debug-server and detailed
timeline variables. A direct launch preserves these if someone explicitly set
them; they remain off when absent.

The new defaults are compiled only when `LICHII_ALLYX_Z1E` is enabled. Generic
builds retain their previous defaults. Existing emulator settings, prepared
shaders and controller mappings are used.
