#!/usr/bin/env bash
# Build all six mupen64plus components for the TrimUI Brick and assemble the
# release into /build/dist.
#
# Flags are validated on-device: OPTFLAGS="-O3 -mcpu=cortex-a53" WITHOUT -flto
# (LTO miscompiles glide64mk2; game resets on START). glide64mk2 also needs
# -Wl,-rpath-link into the sysroot lib dir.
set -euo pipefail

SRC=${SRC:-/build/src}
DIST=${DIST:-/build/dist}
PAT=${PAT:-/build/patches}
SR=${SYSROOT:-/usr/aarch64-linux-gnu}
TC=${CROSS_COMPILE%-}
OPT="-O3 -mcpu=cortex-a53"
JOBS=${JOBS:-2}

INC="-I$SR/usr/include"
INC_SDL="-I$SR/usr/include -I$SR/usr/include/SDL2 -D_REENTRANT"

COMMON=(
  HOST_CPU=aarch64
  PIE=1
  APIDIR="$SRC/mupen64plus-core/src/api"
  PKG_CONFIG=/bin/true
  SDL_CFLAGS="$INC_SDL"
  SDL_LDLIBS="-L$SR/usr/lib -lSDL2"
  TARGET_ARCH="--sysroot=$SR"
  OPTFLAGS="$OPT"
)

log() { echo "== $* =="; }

# Optional patches: patches/<component>.patch applied to src/<component>
apply_patches() {
  [ -d "$PAT" ] || return 0
  for p in "$PAT"/*.patch; do
    [ -f "$p" ] || continue
    local comp
    comp=$(basename "$p" .patch)
    log "applying $(basename "$p") -> $comp"
    ( cd "$SRC/$comp" && git apply "$p" ) || { echo "patch failed: $p"; return 1; }
  done
}

build() {
  local dir=$1 out=$2; shift 2
  local logf="$DIST/log/$(basename "$dir").log"
  mkdir -p "$DIST/log"
  log "building $(basename "$dir")"
  ( cd "$SRC/$dir/projects/unix" \
    && rm -rf _obj && rm -f "$out" \
    && make all -j"$JOBS" "${COMMON[@]}" "$@" >"$logf" 2>&1 ) \
  || { echo "BUILD FAILED: $dir (see $logf)"; tail -30 "$logf"; return 1; }
  [ -e "$SRC/$dir/projects/unix/$out" ] || { echo "no output $out"; return 1; }
}

strip_all() { for f in "$@"; do "$TC-strip" --strip-unneeded "$f"; done; }

# Windows checkouts add CRLF; normalize so bash/make work (no-op on LF)
find "$SRC" -type f \( -name '*.sh' -o -name 'Makefile' -o -name '*.mk' \) \
  -exec sed -i 's/\r$//' {} +

apply_patches

build mupen64plus-core libmupen64plus.so.2.0.0 \
  USE_GLES=1 VULKAN=0 \
  ZLIB_CFLAGS="$INC" ZLIB_LDLIBS="-L$SR/usr/lib -lz" \
  LIBPNG_CFLAGS="$INC/libpng12" LIBPNG_LDLIBS="-L$SR/usr/lib -lpng12"

build mupen64plus-ui-console mupen64plus \
  COREDIR="./" PLUGINDIR="./"

build mupen64plus-audio-sdl mupen64plus-audio-sdl.so
build mupen64plus-input-sdl mupen64plus-input-sdl.so
build mupen64plus-rsp-hle mupen64plus-rsp-hle.so

build mupen64plus-video-glide64mk2 mupen64plus-video-glide64mk2.so \
  USE_GLES=1 \
  ZLIB_CFLAGS="$INC" ZLIB_LDLIBS="-L$SR/usr/lib -lz" \
  LIBPNG_CFLAGS="$INC/libpng12" LIBPNG_LDLIBS="-L$SR/usr/lib -lpng12" \
  GL_CFLAGS="$INC" GL_LDLIBS="-L$SR/usr/lib -lGLESv2" \
  SDL_LDLIBS="-L$SR/usr/lib -lSDL2 -Wl,-rpath-link,$SR/usr/lib"

build mupen64plus-video-rice mupen64plus-video-rice.so \
  USE_GLES=1 \
  ZLIB_CFLAGS="$INC" ZLIB_LDLIBS="-L$SR/usr/lib -lz" \
  GL_CFLAGS="$INC" GL_LDLIBS="-L$SR/usr/lib -lGLESv2" \
  SDL_LDLIBS="-L$SR/usr/lib -lSDL2 -Wl,-rpath-link,$SR/usr/lib"

log "assembling release into $DIST"
rm -rf "$DIST/mupen64plus"
mkdir -p "$DIST/mupen64plus"

U="$SRC/mupen64plus-ui-console/projects/unix"
P="$SRC"
cp "$U/mupen64plus"                            "$DIST/mupen64plus/"
cp -L "$P/mupen64plus-core/projects/unix/libmupen64plus.so.2.0.0" \
                                              "$DIST/mupen64plus/libmupen64plus.so.2"
cp "$P/mupen64plus-audio-sdl/projects/unix/mupen64plus-audio-sdl.so" \
                                              "$DIST/mupen64plus/"
cp "$P/mupen64plus-input-sdl/projects/unix/mupen64plus-input-sdl.so" \
                                              "$DIST/mupen64plus/"
cp "$P/mupen64plus-rsp-hle/projects/unix/mupen64plus-rsp-hle.so" \
                                              "$DIST/mupen64plus/"
cp "$P/mupen64plus-video-glide64mk2/projects/unix/mupen64plus-video-glide64mk2.so" \
                                              "$DIST/mupen64plus/"
cp "$P/mupen64plus-video-rice/projects/unix/mupen64plus-video-rice.so" \
                                              "$DIST/mupen64plus/"

cp "/opt/gcc16-aarch64/aarch64-linux-gnu/lib64/libstdc++.so.6.0.36" \
                                              "$DIST/mupen64plus/libstdc++.so.6"
cp "/opt/gcc16-aarch64/aarch64-linux-gnu/lib64/libgcc_s.so.1" \
                                              "$DIST/mupen64plus/"
cp "$SR/usr/lib/libz.so.1.2.8"                "$DIST/mupen64plus/libz.so.1"

cp "$P/mupen64plus-core/data/mupen64plus.ini"      "$DIST/mupen64plus/"
cp "$P/mupen64plus-core/data/mupencheat.txt"       "$DIST/mupen64plus/"
cp "$P/mupen64plus-core/data/font.ttf"             "$DIST/mupen64plus/"
cp "$P/mupen64plus-input-sdl/data/InputAutoCfg.ini" "$DIST/mupen64plus/"

strip_all "$DIST"/mupen64plus/mupen64plus \
          "$DIST"/mupen64plus/libmupen64plus.so.2 \
          "$DIST"/mupen64plus/*.so \
          "$DIST"/mupen64plus/libstdc++.so.6 \
          "$DIST"/mupen64plus/libgcc_s.so.1 \
          "$DIST"/mupen64plus/libz.so.1

log "done"
ls -la "$DIST/mupen64plus/"
