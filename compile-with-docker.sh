#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------

# Prevent OS: unbound variable error
: "${OS:=}"
# Usage:
#   ./compile-with-docker.sh [Preset] [CMake options...]
# Examples:
#   ./compile-with-docker.sh Custom
#   ./compile-with-docker.sh ApeX
#   ./compile-with-docker.sh ApeX -DENABLE_FEAT_N7SIX_GAME=ON
#   ./compile-with-docker.sh All
# Default preset: "Custom"
# ---------------------------------------------

IMAGE=uvk1-uvk5v3
PRESET=${1:-Custom}
shift || true  # remove preset from arguments if present
CMAKE_PRESETS_FILE=CMakePresets.json

# Any remaining args will be treated as CMake cache variables
EXTRA_ARGS=("$@")

get_available_presets() {
  awk '
    /"buildPresets"[[:space:]]*:/ { in_build = 1; next }
    in_build && /^    \]/ { exit }
    in_build && /"name"[[:space:]]*:/ {
      if (match($0, /"name"[[:space:]]*:[[:space:]]*"[^"]+"/)) {
        line = substr($0, RSTART, RLENGTH)
        sub(/^.*"name"[[:space:]]*:[[:space:]]*"/, "", line)
        sub(/"$/, "", line)
        print line
      }
    }
  ' "$CMAKE_PRESETS_FILE"
}

if [[ ! -f "$CMAKE_PRESETS_FILE" ]]; then
  echo "❌ Missing preset definition file: $CMAKE_PRESETS_FILE"
  exit 1
fi

mapfile -t AVAILABLE_PRESETS < <(get_available_presets)

if [[ ${#AVAILABLE_PRESETS[@]} -eq 0 ]]; then
  echo "❌ No build presets found in $CMAKE_PRESETS_FILE"
  exit 1
fi

is_valid_preset() {
  local preset="$1"
  local available_preset

  [[ "$preset" == "All" ]] && return 0

  for available_preset in "${AVAILABLE_PRESETS[@]}"; do
    if [[ "$available_preset" == "$preset" ]]; then
      return 0
    fi
  done

  return 1
}

# ---------------------------------------------
# Validate preset name
# ---------------------------------------------
if ! is_valid_preset "$PRESET"; then
  echo "❌ Unknown preset: '$PRESET'"
  echo "Valid presets are: ${AVAILABLE_PRESETS[*]}, All"
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

    # Robust Docker path handling for Windows and Git Bash
    local DOCKER_PATH
    if [[ "$OSTYPE" == "msys"* || "$OSTYPE" == "cygwin"* ]]; then
      DOCKER_PATH=$(pwd -W)
    else
      DOCKER_PATH=$(pwd)
    fi
    # Debug: List /src contents before running CMake
    docker run --rm $TTY_FLAG -v "$DOCKER_PATH":/src "$IMAGE" bash -c "ls -l /src"
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
  PRESETS=("${AVAILABLE_PRESETS[@]}")
  for p in "${PRESETS[@]}"; do
    build_preset "$p"
  done
  echo ""
  echo "🎉 All presets built successfully!"
else
  build_preset "$PRESET"
fi