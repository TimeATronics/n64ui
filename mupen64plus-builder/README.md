# mupen64plus-builder

Standalone build system for the Nintendo 64 emulator on the TRIMUI Brick
(tg5040 / Allwinner A133P, aarch64, TinaLinux, PowerVR GLES 3.2).

Everything mupen64plus-related is built from pinned upstream sources with a
custom GCC 16.2.0 cross-toolchain inside a Docker image. Libraries already in
the Brick SDK sysroot (zlib, libpng, SDL2 GE8300, GLES/EGL, glibc) are used
as-is; only the six mupen64plus components are compiled.

## Components (pinned)

| component | version |
|---|---|
| mupen64plus-core | 2.6.0 |
| mupen64plus-ui-console | 2.6.0 |
| mupen64plus-audio-sdl | 2.6.0 |
| mupen64plus-input-sdl | 2.6.0 |
| mupen64plus-rsp-hle | 2.6.0 |
| mupen64plus-video-glide64mk2 | nightly-build (b07cb0b) |

All under https://github.com/mupen64plus/

## Toolchain & sysroot

- GCC 16.2.0 aarch64 cross-compiler, baked directly into the Docker image. You
  supply the tarball: `toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz` (see
  `toolchain/README.md`).
- Brick sysroot (Linaro glibc 2.25 + TrimUI SDK usr/ staging + prebuilt SDL2
  GE8300), downloaded during the image build from
  `trimui/toolchain_sdk_smartpro` release 20231018, sha256-pinned. Archives are
  cached in a BuildKit cache mount, so rebuilds do not re-download;
  `sysroot/download.sh` can also pre-seed `sysroot/cache/` for offline builds.

The image runs on the host architecture and emits aarch64 binaries; no qemu.

## Quick start

```sh
# 1. put the compiler tarball in toolchain/
# 2. check out sources
make setup
# 3. build the image (first run downloads ~400 MB once)
make image
# 4. build the release into dist/mupen64plus/
make release
```

## Publishing the image

```sh
docker login
make publish
# or: make publish REGISTRY_IMAGE=ghcr.io/<you>/mupen64plus-builder-toolchain:gcc16
```

## Release layout (dist/mupen64plus/)

```
mupen64plus                        frontend (ui-console)
libmupen64plus.so.2                core
mupen64plus-video-glide64mk2.so    video plugin (default)
mupen64plus-audio-sdl.so           audio plugin
mupen64plus-input-sdl.so           input plugin
mupen64plus-rsp-hle.so             rsp plugin
libstdc++.so.6                     from GCC 16.2.0 (GLIBCXX_3.4.32; device's is too old)
libgcc_s.so.1                      from GCC 16.2.0
libz.so.1                          from SDK sysroot
mupen64plus.ini / mupencheat.txt / font.ttf   from mupen64plus-core data/
InputAutoCfg.ini                   from mupen64plus-input-sdl data/
```

Deploy by replacing the contents of `/mnt/SDCARD/Emus/N64/mupen64plus/` on the
Brick (keep a backup). Launch through the stock NextUI with a config.json
entry pointing at the directory, or use:

```sh
#!/bin/sh
cd /mnt/SDCARD/Emus/N64/mupen64plus
LD_LIBRARY_PATH=. ./mupen64plus --gfx mupen64plus-video-glide64mk2.so \
  --corelib ./libmupen64plus.so.2 --datadir . --plugindir . "$1"
```

## Important: no LTO

`OPTFLAGS="-O3 -mcpu=cortex-a53"` is used for every component. Do not add
`-flto`: LTO miscompiles glide64mk2 and the game resets to the intro when you
press START. The recipe is baked into `docker/build-all.sh`.

## Patches

Supported, but the default release is unpatched (that is the validated
configuration). See `patches/README.md`.

## Debugging

- Build logs land in `dist/log/`.
- Shell into the image: `docker run --rm -it -v <repo>/src:/build/src -v <repo>/dist:/build/dist <image> bash`
- Inspect a binary: `aarch64-linux-gnu-readelf -d dist/mupen64plus/mupen64plus-video-glide64mk2.so | grep -E 'NEEDED|SONAME'`

## History

The original deployment issue (game resetting at START with a self-built
glide64mk2) was traced to `-flto` in the build flags, not the compiler version
or source lineage.
