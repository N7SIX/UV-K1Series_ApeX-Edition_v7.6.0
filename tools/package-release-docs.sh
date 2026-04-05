#!/bin/bash
#
# =====================================================================================
# Release Documentation Packaging Script
# Purpose: Package standard documentation files into a zip for GitHub release attachments
# Usage: ./tools/package-release-docs.sh [version] [output-dir]
# =====================================================================================
#

set -e

# Configuration
VERSION="${1:?Version required (e.g., v7.6.5br3)}"
OUTPUT_DIR="$(cd "${2:-.}" && pwd)"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Documentation files to include
MANUAL_FILE="${REPO_ROOT}/Documentation/Owner's Manual - ApeX Edition.md"
SPECTRUM_GUIDE="${REPO_ROOT}/Documentation/SPECTRUM_ANALYZER_GUIDE.md"

# Find the latest release note matching the version
LATEST_RELEASE_NOTE=$(find "${REPO_ROOT}/Documentation/release-notes" -maxdepth 1 -name "${VERSION}*-apex.md" | sort -V | tail -1)

if [[ -z "$LATEST_RELEASE_NOTE" ]]; then
    echo "ERROR: No release note found for version ${VERSION}" >&2
    exit 1
fi

if [[ ! -f "$MANUAL_FILE" ]]; then
    echo "ERROR: Owner's Manual not found at ${MANUAL_FILE}" >&2
    exit 1
fi

if [[ ! -f "$SPECTRUM_GUIDE" ]]; then
    echo "ERROR: Spectrum Analyzer Guide not found at ${SPECTRUM_GUIDE}" >&2
    exit 1
fi

# Create temporary directory for packaging
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Create documentation directory inside temp
DOC_DIR="${TEMP_DIR}/ApeX-${VERSION}_Documentation"
mkdir -p "$DOC_DIR"

# Copy documentation files
cp "$MANUAL_FILE" "$DOC_DIR/"
cp "$SPECTRUM_GUIDE" "$DOC_DIR/"
cp "$LATEST_RELEASE_NOTE" "$DOC_DIR/"

# Create zip file
ZIP_FILE="${OUTPUT_DIR}/ApeX-${VERSION}_Documentation.zip"
cd "$TEMP_DIR"
zip -r "$ZIP_FILE" "ApeX-${VERSION}_Documentation" > /dev/null 2>&1

# Display result
echo "✓ Release documentation package created:"
echo "  File: $ZIP_FILE"
echo "  Size: $(du -h "$ZIP_FILE" | cut -f1)"
echo ""
echo "  Contents:"
echo "  - $(basename "$MANUAL_FILE")"
echo "  - $(basename "$SPECTRUM_GUIDE")"
echo "  - $(basename "$LATEST_RELEASE_NOTE")"
echo ""
echo "Ready for GitHub release attachment."
