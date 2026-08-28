#!/usr/bin/env bash
# Build n64ui with the docker toolchain image (mupen64plus-builder).
# Resolves the image like mupen64plus-builder's build-release.sh:
# local image, else build from source, else pull the published one.
# The m64p api headers come from the builder submodule (mounted read-only).
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=${IMAGE:-mupen64plus-builder/toolchain:gcc16}
REGISTRY_IMAGE=${REGISTRY_IMAGE:-docker.io/aradhyac9/mupen64plus-builder-toolchain:gcc16}
API_HOST=$(cd ../mupen64plus-builder/src/mupen64plus-core/src/api && pwd)
BUILDER_TAR=../mupen64plus-builder/toolchain/gcc16-aarch64-linux-gnu-cross.tar.gz

if docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "using local image: $IMAGE"
elif [ -f "$BUILDER_TAR" ]; then
  echo "image not local, building from source (../mupen64plus-builder)"
  ( cd ../mupen64plus-builder && bash scripts/build-image.sh "$IMAGE" )
else
  echo "image not local, pulling: $REGISTRY_IMAGE"
  docker pull "$REGISTRY_IMAGE"
  docker tag "$REGISTRY_IMAGE" "$IMAGE"
fi

echo "== building n64ui in container =="
docker run --rm \
  -u "$(id -u):$(id -g)" \
  -v "$PWD":/build \
  -v "$API_HOST":/build-api:ro \
  "$IMAGE" \
  bash -c 'make -C /build all API_INC=/build-api \
    CROSS_COMPILE=/opt/gcc16-aarch64/bin/aarch64-linux-gnu- \
    SYSROOT=/usr/aarch64-linux-gnu'

echo "== done: $(pwd)/dist/n64ui =="
