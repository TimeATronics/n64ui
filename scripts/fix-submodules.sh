#!/bin/bash
# Re-create the git metadata for the builder's src/* submodule checkouts.
# The original repos lived under mupen64plus-builder/.git/modules which was
# removed when the root repo was created, leaving dangling .git files.
# Content is preserved as-is; history is recreated as a single snapshot.
# The root repo's .gitmodules is updated with the upstream URLs.
set -euo pipefail
cd "$(dirname "$0")/.."

declare -A URLS
URLS[core]="https://github.com/mupen64plus/mupen64plus-core"
URLS[ui-console]="https://github.com/mupen64plus/mupen64plus-ui-console"
URLS[audio-sdl]="https://github.com/mupen64plus/mupen64plus-audio-sdl"
URLS[input-sdl]="https://github.com/mupen64plus/mupen64plus-input-sdl"
URLS[rsp-hle]="https://github.com/mupen64plus/mupen64plus-rsp-hle"
URLS[glide64mk2]="https://github.com/mupen64plus/mupen64plus-video-glide64mk2"
URLS[rice]="https://github.com/mupen64plus/mupen64plus-video-rice"

# directory name -> comp name (for the URL map)
declare -A COMP
COMP[mupen64plus-core]=core
COMP[mupen64plus-ui-console]=ui-console
COMP[mupen64plus-audio-sdl]=audio-sdl
COMP[mupen64plus-input-sdl]=input-sdl
COMP[mupen64plus-rsp-hle]=rsp-hle
COMP[mupen64plus-video-glide64mk2]=glide64mk2
COMP[mupen64plus-video-rice]=rice

for dir in "${!COMP[@]}"; do
  comp=${COMP[$dir]}
  dir="mupen64plus-builder/src/$dir"
  echo "== $dir =="
  # Remove the dangling .git pointer FILE if present (it references the
  # deleted builder .git/modules and is unresolvable); content untouched.
  if [ -f "$dir/.git" ]; then
    echo "  removing dangling .git pointer"
    rm -f "$dir/.git"
  fi
  # Always init: git init in a subdir of the root repo creates a nested repo
  # (never skip via rev-parse - it resolves the parent repo).
  echo "  init"
  ( cd "$dir" && git init -q -b main )
  ( cd "$dir"
    git add -A .
    git -c user.name=brick -c user.email=brick@local commit -qm "snapshot for TrimUI Brick toolchain" \
      || echo "  (no new content)"
    git remote remove origin 2>/dev/null || true
    git remote add origin "${URLS[$comp]}"
  )
done

# Register all as submodules of the ROOT repo.
: > .gitmodules
for name in "${!COMP[@]}"; do
  cat >> .gitmodules <<EOF
[submodule "mupen64plus-builder/src/$name"]
	path = mupen64plus-builder/src/$name
	url = ${URLS[${COMP[$name]}]}
EOF
done

git add .gitmodules
for name in "${!COMP[@]}"; do
  git add "mupen64plus-builder/src/$name"
done

echo "== done. root status:"
git status --short | head -20
