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

for comp in core ui-console audio-sdl input-sdl rsp-hle glide64mk2 rice; do
  dir="mupen64plus-builder/src/mupen64plus-$comp"
  echo "== $dir =="
  # Remove the dangling .git pointer FILE (it references the deleted
  # builder .git/modules and is unresolvable); content is untouched.
  if [ -f "$dir/.git" ]; then
    echo "  removing dangling .git pointer"
    rm -f "$dir/.git"
  fi
  if ! git -C "$dir" rev-parse --git-dir >/dev/null 2>&1; then
    echo "  init"
    git -C "$dir" init -q -b main
  fi
  # move the worktree files into the index (preserves content, one snapshot)
  ( cd "$dir" && git rm -rq --cached . 2>/dev/null || true
    git add -A
    git -c user.name=brick -c user.email=brick@local commit -qm "snapshot for TrimUI Brick toolchain"
    git remote remove origin 2>/dev/null || true
    git remote add origin "${URLS[$comp]}"
  )
done

# Register all as submodules of the ROOT repo.
ROOT=$(pwd)
: > .gitmodules
for comp in core ui-console audio-sdl input-sdl rsp-hle glide64mk2 rice; do
  cat >> .gitmodules <<EOF
[submodule "mupen64plus-builder/src/mupen64plus-$comp"]
	path = mupen64plus-builder/src/mupen64plus-$comp
	url = ${URLS[$comp]}
EOF
done

git add .gitmodules
for comp in core ui-console audio-sdl input-sdl rsp-hle glide64mk2 rice; do
  git add "mupen64plus-builder/src/mupen64plus-$comp"
done

echo "== done. git status:"
git status --short | head -15
