#!/usr/bin/env bash
# Static-analysis helper for the ApeX firmware.
# Runs cppcheck if available; optionally runs clang-tidy if configured.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

has() { command -v "$1" >/dev/null 2>&1; }

if has cppcheck; then
    echo "==> Running cppcheck..."
    cppcheck \
        --project="$ROOT_DIR/build/compile_commands.json" \
        --enable=all \
        --suppress=missingIncludeSystem \
        --inline-suppr \
        "$ROOT_DIR/App" "$ROOT_DIR/Core" "$ROOT_DIR/Drivers" "$ROOT_DIR/Middlewares" || true
else
    echo "cppcheck not found; install it to run static analysis."
fi

if has clang-tidy; then
    echo "==> Running clang-tidy..."
    # Requires compile_commands.json (enabled in top-level CMake).
    clang-tidy -p "$ROOT_DIR/build" "$ROOT_DIR/App"/*.c "$ROOT_DIR/Core"/*.c || true
else
    echo "clang-tidy not found; install it to run static analysis."
fi

echo "==> Static analysis complete."