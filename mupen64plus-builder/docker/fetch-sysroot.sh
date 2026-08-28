#!/usr/bin/env bash
# Download the Brick sysroot archives into $1 (a BuildKit cache mount) if
# missing or corrupt. Pre-seeds from $2 (a COPYed host cache) when present.
set -euo pipefail

DEST=${1:?usage: fetch-sysroot.sh <dest-dir> [seed-dir]}
SEED=${2:-}

BASE=https://github.com/trimui/toolchain_sdk_smartpro/releases/download/20231018
FILES=(
  "aarch64-linux-gnu-7.5.0-linaro.tgz|604d97c1f5fadc88d6ebff577ea648d0fbc650411af98a3bd8783dbe4e3ed154"
  "SDK_usr_tg5040_a133p.tgz|b6b615d03204e9d9bb1a91c31de0c3402434f6b8fc780743fa88a5f1da6f3c79"
  "SDL2-2.26.1.GE8300.tgz|a37fbed6c8771f1050e6520fa9c48354e01519de813fe176ae53089f2087bb87"
)

mkdir -p "$DEST"

for entry in "${FILES[@]}"; do
  name=${entry%%|*}
  want=${entry##*|}
  file="$DEST/$name"

  if [ -f "$file" ] && echo "$want  $file" | sha256sum -c - >/dev/null 2>&1; then
    echo "sysroot cache hit:  $name"
    continue
  fi

  if [ -n "$SEED" ] && [ -f "$SEED/$name" ] && \
     echo "$want  $SEED/$name" | sha256sum -c - >/dev/null 2>&1; then
    echo "seeding from host cache:  $name"
    cp "$SEED/$name" "$file"
    continue
  fi

  echo "downloading:  $name"
  curl -fL --retry 3 -o "$file.part" "$BASE/$name"
  echo "$want  $file.part" | sha256sum -c -
  mv "$file.part" "$file"
done
