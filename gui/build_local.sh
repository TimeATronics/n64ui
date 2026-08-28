#!/usr/bin/env bash
# Direct WSL cross-build for fast iteration (no docker).
# Produces dist/n64ui; deploy to the Brick to test.
set -euo pipefail
cd "$(dirname "$0")"
make local
