#!/usr/bin/env bash
# Build + push the toolchain image to a registry (docker.io).
# Must be logged in first: docker login
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=${1:?usage: publish-image.sh <registry>/<owner>/<image>:<tag>}

[ -f toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz ] || {
  echo "ERROR: toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz not found."
  exit 1
}

bash sysroot/download.sh

docker buildx build --push --progress=plain -f docker/Dockerfile -t "$IMAGE" .
echo "pushed: $IMAGE"
