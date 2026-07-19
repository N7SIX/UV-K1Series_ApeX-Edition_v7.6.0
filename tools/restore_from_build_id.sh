#!/usr/bin/env bash
# Restores the source tree to a specific build ID snapshot.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [ ! -f "${SRC_DIR}/CMakeLists.txt" ]; then
    echo "❌ Cannot locate repository root from $(dirname "$0")" >&2
    exit 1
fi

ARCHIVE_DIR="${SRC_DIR}/archive/builds"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <BUILD_ID>"
    echo
    echo "Available restore points:"
    if [ -d "${ARCHIVE_DIR}" ]; then
        ls -1 "${ARCHIVE_DIR}" 2>/dev/null | sed 's/^/  - /' || echo "  (none)"
    else
        echo "  (no archive directory found)"
    fi
    exit 1
fi

# Allow user to pass either the 8-hex-digit BUILD_ID shown in SysInf (e.g. "6a5c8274")
# or the full restore-point directory name "build_id-6a5c8274".
RAW_BUILD_ID="$1"
if [[ "${RAW_BUILD_ID}" =~ ^build_id- ]]; then
    BUILD_ID="${RAW_BUILD_ID#build_id-}"
    RESTORE_DIR="${ARCHIVE_DIR}/${RAW_BUILD_ID}"
else
    BUILD_ID="${RAW_BUILD_ID}"
    RESTORE_DIR="${ARCHIVE_DIR}/build_id-${BUILD_ID}"
fi

if [ ! -d "${RESTORE_DIR}" ]; then
    echo "❌ Restore point not found: ${BUILD_ID}"
    echo
    echo "Available restore points:"
    if [ -d "${ARCHIVE_DIR}" ]; then
        ls -1 "${ARCHIVE_DIR}" 2>/dev/null | sed 's/^/  - /' || echo "  (none)"
    else
        echo "  (no archive directory found)"
    fi
    exit 1
fi

if [ ! -f "${RESTORE_DIR}/manifest.txt" ]; then
    echo "❌ Restore point is missing manifest: ${BUILD_ID}" >&2
    exit 1
fi

SNAPSHOT_INCLUDE_BUILD="$(grep '^INCLUDE_BUILD=' "${RESTORE_DIR}/manifest.txt" 2>/dev/null | cut -d'=' -f2 | tr '[:upper:]' '[:lower:]' || true)"
if [ "${SNAPSHOT_INCLUDE_BUILD}" = "true" ] && [ -d "${RESTORE_DIR}/build" ]; then
    RESTORE_BUILD=true
else
    RESTORE_BUILD=false
fi

# Warn about potential uncommitted changes before destructive restore
if command -v git >/dev/null 2>&1 && [ -d "${SRC_DIR}/.git" ]; then
    cd "${SRC_DIR}"
    if ! git diff --quiet --exit-code 2>/dev/null || ! git diff --cached --quiet --exit-code 2>/dev/null; then
        echo "⚠️  WARNING: Repository has uncommitted changes that will be overwritten!" >&2
        echo "   Commit or stash your changes before restoring, or proceed at your own risk." >&2
    fi
    cd - >/dev/null
fi

echo "🔄 Restoring source to BUILD_ID=${BUILD_ID} ..."
echo "   Source snapshot: ${RESTORE_DIR}"
if [ "${RESTORE_BUILD}" = true ]; then
    echo "   Build folder restore: enabled (snapshot includes build)"
else
    echo "   Build folder restore: skipped (snapshot has no build folder)"
fi

# Use a temporary staging directory for atomic-like restore safety
TMP_RESTORE="${SRC_DIR}.restore.partial"
if [ -d "${TMP_RESTORE}" ]; then
    rm -rf "${TMP_RESTORE}"
fi
mkdir -p "${TMP_RESTORE}"

# Copy restore point contents to temp dir
if ! cp -a "${RESTORE_DIR}/"* "${TMP_RESTORE}/" 2>/dev/null; then
    echo "❌ Failed to copy restore point files. Aborting." >&2
    rm -rf "${TMP_RESTORE}"
    exit 1
fi

# Remove current source files (except protected dirs), then replace with restored snapshot
# This avoids rsync entirely, which has permission issues on Windows/MINGW64
PROTECTED=".git archive .vscode tmp tools"
if [ "${RESTORE_BUILD}" != true ]; then
    PROTECTED="build ${PROTECTED}"
fi

# Remove unprotected top-level items from source
for item in "${SRC_DIR}"/*; do
    name="$(basename "$item")"
    keep=false
    for p in ${PROTECTED}; do
        if [ "$name" = "$p" ]; then
            keep=true
            break
        fi
    done
    if [ "$keep" = false ]; then
        rm -rf "$item"
    fi
done

# Copy restored snapshot back to source
if ! cp -a "${TMP_RESTORE}/"* "${SRC_DIR}/" 2>/dev/null; then
    echo "❌ Failed to copy restored files back to source. Aborting." >&2
    rm -rf "${TMP_RESTORE}"
    exit 1
fi

# Verify restore integrity against checksums
echo "🔍 Verifying restore integrity..."
if [ -f "${RESTORE_DIR}/SHA256SUMS" ]; then
    cd "${SRC_DIR}"
    if sha256sum -c "${RESTORE_DIR}/SHA256SUMS" >/dev/null 2>&1; then
        echo "✅ Integrity check passed (SHA256)"
    else
        echo "⚠️  Warning: Restore integrity check failed. Files may have been modified." >&2
    fi
    cd - >/dev/null
elif [ -f "${RESTORE_DIR}/manifest.txt" ]; then
    # Fallback: verify file counts
    BACKUP_COUNT=$(grep "^BACKUP_FILE_COUNT=" "${RESTORE_DIR}/manifest.txt" | cut -d'=' -f2)
    CURRENT_COUNT=$(find "${SRC_DIR}" -type f \
        ! -path "${SRC_DIR}/build/*" \
        ! -path "${SRC_DIR}/.git/*" \
        ! -path "${SRC_DIR}/archive/builds/*" \
        | wc -l)
    
    if [ "${BACKUP_COUNT}" = "${CURRENT_COUNT}" ]; then
        echo "✅ File count verified: ${CURRENT_COUNT} files"
    else
        echo "⚠️  Warning: File count mismatch. Expected ${BACKUP_COUNT}, got ${CURRENT_COUNT}" >&2
    fi
fi

rm -rf "${TMP_RESTORE}"

echo "✅ Restore complete."
echo "   Manifest for this restore point:"
sed 's/^/   - /' "${RESTORE_DIR}/manifest.txt" || true