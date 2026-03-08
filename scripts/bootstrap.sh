#!/usr/bin/env sh

set -eu

# This bootstrap installs required tools and builds/runs the game on Linux/macOS.

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"

"$SCRIPT_DIR/setup.sh"

cd "$PROJECT_ROOT"
zig build setup
zig build run
