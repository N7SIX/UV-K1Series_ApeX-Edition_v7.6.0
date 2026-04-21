#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------

# Prevent OS: unbound variable error
: "${OS:=}"
# Usage:
#   ./compile-with-docker.sh [Preset] [CMake options...]
# Examples:
#   ./compile-with-docker.sh Custom
#   ./compile-with-docker.sh Bandscope -DENABLE_SPECTRUM=ON
#   ./compile-with-docker.sh Broadcast -DENABLE_FEAT_N7SIX_GAME=ON -DENABLE_NOAA=ON
#   ./compile-with-docker.sh All
# Default preset: "Custom"
# ---------------------------------------------

IMAGE=uvk1-uvk5v3
PRESET=${1:-Custom}
shift || true  # remove preset from arguments if present

# Any remaining args will be treated as CMake cache variables
EXTRA_ARGS=("$@")

# ---------------------------------------------
# Validate preset name
# ---------------------------------------------
if [[ ! "$PRESET" =~ ^(Custom|Bandscope|Broadcast|Basic|RescueOps|Game|ApeX|All)$ ]]; then
  echo "❌ Unknown preset: '$PRESET'"
  echo "Valid presets are: Custom, Bandscope, Broadcast, Basic, RescueOps, Game, ApeX, All"
  exit 1
fi

# ---------------------------------------------
# Build the Docker image (only needed once)
# ---------------------------------------------
docker build -t "$IMAGE" .

# ---------------------------------------------
# Clean existing CMake cache to ensure toolchain reload
# ---------------------------------------------
rm -rf build

# ---------------------------------------------
# Function to build one preset
# ---------------------------------------------
build_preset() {
  local preset="$1"
  echo ""
  echo "=== 🚀 Building preset: ${preset} ==="
  echo "---------------------------------------------"

  # FIX: Detect if we are in a real terminal (TTY)
  # If -t 0 is true, we use -it. If false (like in GitHub Actions), we use -i.
  local TTY_FLAG
  if [ -t 0 ]; then
    TTY_FLAG="-it"
  else
    TTY_FLAG="-i"
  fi

  # Detect environment and use correct path for Docker
  local DOCKER_PATH
  if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* ]]; then
    # If running in Git Bash, try to detect if launched from Windows Terminal/PowerShell/CMD
    if [ -n "$WT_SESSION" ] || [ -n "$TERM_PROGRAM" ] || [ ! -z "$ComSpec" ]; then
      # Use Windows path for Docker
      DOCKER_PATH="/c/Users/sebue/Documents/N7SIX/Software/Quansheng/httpsgithub.comN7SIXUV-K1Series_ApeX-Edition_v7.6.0/UV-K1Series_ApeX-Edition_v7.6.0"
    else
      # Use Git Bash path conversion
      DOCKER_PATH=$(pwd -W | sed 's|\\|/|g; s|^\([A-Za-z]\):|/\L\1|')
    fi
  elif [[ "$OS" == "Windows_NT" ]]; then
    # PowerShell or CMD
    DOCKER_PATH="$PWD"
  else
    DOCKER_PATH=$(pwd)
  fi
  docker run --rm $TTY_FLAG -v "$DOCKER_PATH":/src "$IMAGE" \
    bash -c "which arm-none-eabi-gcc && arm-none-eabi-gcc --version && \
             cmake --preset ${preset} ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} && \
             cmake --build --preset ${preset} -j"
  echo "✅ Done: ${preset}"
}

# ---------------------------------------------
# Handle 'All' preset
# ---------------------------------------------
if [[ "$PRESET" == "All" ]]; then
  PRESETS=(Bandscope Broadcast Basic RescueOps Game ApeX)
  for p in "${PRESETS[@]}"; do
    build_preset "$p"
  done
  echo ""
  echo "🎉 All presets built successfully!"
else
  build_preset "$PRESET"
fi