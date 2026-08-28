# Patches

The default release is built from unpatched pinned upstream sources; that is
the configuration validated on-device.

To apply a patch, drop a file named `<component>.patch` directly in this
directory (no subdirectories), e.g. `mupen64plus-ui-console.patch`. The build
applies each `*.patch` to the matching submodule under `src/` with `git apply`
before compiling.

`optional/` contains patches carried over from the minui-n64-pak project
(they apply cleanly to our pinned 2.6.0 sources):

- `optional/mupen64plus-core.patch`
- `optional/mupen64plus-ui-console.patch` (romfilename support for the pak frontend)
- `optional/mupen64plus-input-sdl.patch`

They are intentionally not enabled: they target the minui frontend, which this
build system does not use. Copy one into this directory to enable it.
