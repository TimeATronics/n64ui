#!/usr/bin/env bash
# N64UI release build tool for the TrimUI Brick.
#
#   bash build.sh release        build everything from scratch and tar it as
#                                dist/release.tar.gz:
#                                  1. mupen64plus-builder (docker toolchain)
#                                     -> core + audio/input/rsp/glide64mk2/
#                                        rice plugins
#                                  2. gui (docker toolchain) -> n64ui frontend
#                                  3. assemble the device directory structure
#                                     (Emus/N64/...) from the build output +
#                                     release-assets/, strip, tar
#   bash build.sh release-clean  undo a release build: reverse applied source
#                                patches, kill stray build containers, remove
#                                all build output dirs and the tarball.
set -euo pipefail
cd "$(dirname "$0")"

CMD="${1:-release}"
DIST=dist
STAGE="$DIST/release/Emus/N64"
ASSET="release-assets/Emus/N64"
BUILDER_DIST="mupen64plus-builder/dist/mupen64plus"
RICE_DIR="mupen64plus-builder/src/mupen64plus-video-rice"
RICE_PATCH="$PWD/mupen64plus-builder/patches/mupen64plus-video-rice.patch"

# host-side aarch64 strip tool (WSL toolchain)
STRIP=$(command -v aarch64-linux-gnu-strip || true)
if [ -z "${STRIP:-}" ]; then
  for c in /home/aradhya/toolchains/gcc16-aarch64/bin/aarch64-linux-gnu-strip \
           /opt/gcc16-aarch64/bin/aarch64-linux-gnu-strip; do
    [ -x "$c" ] && STRIP=$c && break
  done
fi

release() {
  echo "== 1/3 building mupen64plus (core + plugins) via docker =="
  ( cd mupen64plus-builder && make release )

  echo "== 2/3 building n64ui via docker =="
  ( cd gui && make release )

  echo "== 3/3 assembling release =="
  rm -rf "$DIST/release"
  mkdir -p "$STAGE/n64ui-test"

  # -- payload (Emus/N64/n64ui-test/) ----------------------------------------
  B="$BUILDER_DIST"
  cp gui/dist/n64ui                  "$STAGE/n64ui-test/"
  cp "$B/libmupen64plus.so.2"        "$STAGE/n64ui-test/"
  cp "$B"/mupen64plus-*.so           "$STAGE/n64ui-test/"
  cp "$B/libstdc++.so.6" "$B/libgcc_s.so.1" "$B/libz.so.1" "$STAGE/n64ui-test/"
  cp "$B/mupen64plus.ini" "$B/mupencheat.txt" "$B/font.ttf" \
     "$B/InputAutoCfg.ini"           "$STAGE/n64ui-test/"
  cp "$ASSET/n64ui-test/Glide64mk2.ini"     "$STAGE/n64ui-test/"
  cp "$ASSET/n64ui-test/RiceVideoLinux.ini" "$STAGE/n64ui-test/"

  # -- Emus/N64 root (configs + scripts) --------------------------------------
  cp "$ASSET/config.json"         "$STAGE/"
  cp "$ASSET/n64ui_glide64mk2.sh" "$STAGE/"
  cp "$ASSET/n64ui_rice.sh"       "$STAGE/"
  cp "$ASSET/cpufreq-OLD.sh"      "$STAGE/"
  cp "$ASSET/mupen64plus.cfg"     "$STAGE/"

  chmod +x "$STAGE"/*.sh

  # -- strip (plugins/libs are stripped by the builder already; n64ui is not) -
  if [ -n "${STRIP:-}" ]; then
    echo "stripping n64ui with $STRIP"
    "$STRIP" --strip-unneeded "$STAGE/n64ui-test/n64ui"
  else
    echo "WARNING: no aarch64 strip tool found; n64ui left unstripped"
  fi

  tar czf "$DIST/release.tar.gz" -C "$DIST/release" .
  echo "== done: $PWD/$DIST/release.tar.gz"
  du -h "$DIST/release.tar.gz"
}

release-clean() {
  # Reverse applied source patches (only when they are applied).
  if [ -d "$RICE_DIR" ]; then
    if ( cd "$RICE_DIR" && git apply --reverse --check "$RICE_PATCH" ); then
      echo "reversing $RICE_PATCH"
      ( cd "$RICE_DIR" && git apply -R "$RICE_PATCH" )
    else
      echo "rice patch not applied (nothing to undo)"
    fi
  fi
  # Kill stray build containers.
  local running
  running=$(docker ps -q 2>/dev/null || true)
  if [ -n "$running" ]; then
    echo "stopping build containers"
    docker rm -f $running
  fi
  # Remove build output dirs and the tarball.
  rm -rf "$DIST" gui/dist mupen64plus-builder/dist
  echo "== release build cleaned =="
}

case "$CMD" in
  release) release ;;
  release-clean) release-clean ;;
  *)
    echo "usage: bash build.sh release | release-clean" >&2
    exit 1
    ;;
esac
