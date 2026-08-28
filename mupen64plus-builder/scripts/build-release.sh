#!/usr/bin/env bash
# Build the mupen64plus release for the TrimUI Brick using docker and assemble
# it into dist/mupen64plus/.
#
# Image resolution:
#   1. use the local image if it exists
#   2. else build it from source (requires toolchain/*.tar.gz)
#   3. else pull the published image from the registry
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=${IMAGE:-${1:-mupen64plus-builder/toolchain:gcc16}}
REGISTRY_IMAGE=${REGISTRY_IMAGE:-docker.io/aradhyac9/mupen64plus-builder-toolchain:gcc16}

git submodule update --init --recursive

if docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "using local image: $IMAGE"
elif [ -f toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz ]; then
  echo "image not local, building from source"
  bash scripts/build-image.sh "$IMAGE"
else
  echo "image not local, pulling: $REGISTRY_IMAGE"
  docker pull "$REGISTRY_IMAGE"
  docker tag "$REGISTRY_IMAGE" "$IMAGE"
fi

mkdir -p dist
docker run --rm \
  -v "$PWD/src":/build/src \
  -v "$PWD/patches":/build/patches:ro \
  -v "$PWD/dist":/build/dist \
  "$IMAGE" /opt/build-all.sh

echo "release ready: $(pwd)/dist/mupen64plus/"
