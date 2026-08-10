#!/usr/bin/env bash
# Creates a restore point (source snapshot) keyed by the build ID
# Used by CMakeLists.txt and compile-with-docker.sh
set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ ! -f "${SRC_DIR}/CMakeLists.txt" ]; then
    echo "❌ Cannot locate repository root" >&2
    exit 1
fi

ARCHIVE_DIR="${SRC_DIR}/archive/builds"
mkdir -p "${ARCHIVE_DIR}"

INCLUDE_BUILD="${INCLUDE_BUILD:-false}"

# Read the exact BUILD_COMMIT that CMake exported during configure.
# BUILD_COMMIT is created first by CMake; BUILD_ID must reuse the same value.
BUILD_COMMIT="unknown"
for candidate in \
    "${SRC_DIR}/build/ApeX/BUILD_COMMIT.txt" \
    "${SRC_DIR}/build/BUILD_COMMIT.txt" \
    "${SRC_DIR}/.build_commit" \
    "${SRC_DIR}/build_id.txt"
do
    if [ -f "${candidate}" ]; then
        CANDIDATE="$(grep -E '^[0-9a-f]{6,}$' "${candidate}" 2>/dev/null | head -n1 || true)"
        if [ -z "${CANDIDATE}" ]; then
            CANDIDATE="$(grep -E 'BUILD_COMMIT=' "${candidate}" 2>/dev/null | head -n1 | cut -d'=' -f2 | tr -d '[:space:]"' || true)"
        fi
        if [ -n "${CANDIDATE}" ]; then
            BUILD_COMMIT="$(echo "${CANDIDATE}" | head -c 12)"
            break
        fi
    fi
done

RESTORE_DIR="${ARCHIVE_DIR}/build_id-${BUILD_COMMIT}"

# Refuse to overwrite existing restore point
if [ -d "${RESTORE_DIR}" ]; then
    echo "⚠️  Restore point already exists: build_id-${BUILD_COMMIT}" >&2
    echo "   Remove ${RESTORE_DIR} manually to recreate." >&2
    exit 1
fi

mkdir -p "${RESTORE_DIR}"

# Write manifest
{
    echo "BUILD_ID=${BUILD_COMMIT}"
    echo "DATE=$(date '+%Y-%m-%d')"
    echo "TIME=$(date '+%H:%M:%S %Z')"
    echo "HOST=$(hostname 2>/dev/null || echo unknown)"
    echo "PRESET=${PRESET:-unknown}"
    echo "INCLUDE_BUILD=${INCLUDE_BUILD}"
    echo "SRC_DIR=${SRC_DIR}"
} > "${RESTORE_DIR}/manifest.txt"

TMP_COPY="${RESTORE_DIR}.partial"
mkdir -p "${TMP_COPY}"

# Copy with exclusions
copy_ok=false
if command -v rsync >/dev/null 2>&1; then
    RSYNC_EXCLUDES=(
        --exclude='.git'
        --exclude='archive'
        --exclude='.vscode'
        --exclude='.clang-format'
        --exclude='.clangd'
        --exclude='compile_commands.json'
        --exclude='.vs'
        --exclude='tmp'
    )
    if [ "${INCLUDE_BUILD}" != "true" ]; then
        RSYNC_EXCLUDES+=(--exclude='build')
    fi
    if rsync -a \
        "${RSYNC_EXCLUDES[@]}" \
        "${SRC_DIR}/" "${TMP_COPY}/"; then
        copy_ok=true
    fi
else
    if command -v cp >/dev/null 2>&1; then
        copy_ok=true
        for item in "${SRC_DIR}"/*; do
            name="$(basename "$item")"
            case "${name}" in
                .git|.vscode|.clang-format|.clangd|compile_commands.json|.vs|tmp)
                    echo "  ⏭️  Skipping: ${name}"
                    continue
                    ;;
                archive)
                    echo "  ⏭️  Skipping: archive"
                    continue
                    ;;
                build)
                    if [ "${INCLUDE_BUILD}" != "true" ]; then
                        echo "  ⏭️  Skipping: build"
                        continue
                    fi
                    ;;
            esac
            if ! cp -a "$item" "${TMP_COPY}/" 2>/dev/null; then
                echo "⚠️  Failed to copy: ${name}" >&2
                copy_ok=false
            fi
        done
    fi
fi

if [ "${copy_ok}" = false ]; then
    echo "❌ Failed to copy source files" >&2
    rm -rf "${TMP_COPY}" "${RESTORE_DIR}"
    exit 1
fi

# Atomic move with Windows-friendly retry (file locks from Defender/Search
# can hold the freshly-copied files for a short time).
move_ok=false
if command -v mv >/dev/null 2>&1; then
    for attempt in 1 2 3 4 5; do
        rm -rf "${RESTORE_DIR}.final"
        sleep 1
        if mv "${TMP_COPY}" "${RESTORE_DIR}.final" >/dev/null 2>&1; then
            if mv "${RESTORE_DIR}.final" "${RESTORE_DIR}" >/dev/null 2>&1; then
                move_ok=true
                break
            fi
        fi
        sleep 1
    done
fi

if [ "${move_ok}" = false ]; then
    echo "⚠️  Restore point skipped (Windows file lock?). Continuing build..." >&2
    rm -rf "${TMP_COPY}" "${RESTORE_DIR}" "${RESTORE_DIR}.final"
fi

# Quick verification (only if restore point was actually created)
if [ "${move_ok}" = true ]; then
    if [ ! -d "${RESTORE_DIR}" ] || [ ! -f "${RESTORE_DIR}/manifest.txt" ]; then
        echo "❌ Verification failed" >&2
        rm -rf "${RESTORE_DIR}"
        exit 1
    fi
    echo "✅ Restore point created: ${RESTORE_DIR}"
    echo "   BUILD_ID=${BUILD_COMMIT}"
    echo "   (matches SysInf BUILD display)"
    ls -1 "${RESTORE_DIR}" | head -5 | sed 's/^/   - /'
else
    echo "   (no restore point — build continues)"
fi