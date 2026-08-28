#!/usr/bin/env bash
# Build the toolchain image locally.
# Usage: scripts/build-image.sh [image-tag]
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=${1:-mupen64plus-builder/toolchain:gcc16}

[ -f toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz ] || {
  echo "ERROR: toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz not found."
  echo "Place the GCC 16.2.0 cross-compiler tarball there first (see toolchain/README.md)."
  exit 1
}

bash sysroot/download.sh

docker buildx build --progress=plain -f docker/Dockerfile -t "$IMAGE" .
echo "done: $IMAGE"
