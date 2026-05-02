#!/bin/bash
#
# Quick Release Helper — Packages complete release ZIP and uploads to GitHub release
# Usage: ./tools/quick-release.sh <VERSION> <TITLE> <NOTES_FILE>
#
# Example:
#   ./tools/quick-release.sh v7.6.5br3 "v7.6.5br3 Spectrum Fixes" release-notes.txt
#

set -e

VERSION="${1:?Version required (e.g., v7.6.5br3)}"
TITLE="${2:?Release title required}"
NOTES_FILE="${3:?Release notes file required}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"

echo "📦 ApeX Release Helper v1.0"
echo ""

# Step 1: Package complete release
echo "Step 1️⃣  Packaging complete release..."
"$REPO_ROOT/tools/package-release-complete.sh" "$VERSION" "$BUILD_DIR"
ZIP_FILE="$BUILD_DIR/ApeX-${VERSION}-Complete-Release.zip"

# Step 2: Verify release file exists
echo ""
echo "Step 2️⃣  Verifying release files..."
if [[ ! -f "$ZIP_FILE" ]]; then
    echo "❌ ERROR: Zip file not found at $ZIP_FILE" >&2
    exit 1
fi
echo "✓ Complete package: $ZIP_FILE"

# Step 3: Verify GitHub release exists
echo ""
echo "Step 3️⃣  Checking GitHub release..."
if ! gh release view "$VERSION" > /dev/null 2>&1; then
    echo "⚠️  Release $VERSION does not exist yet on GitHub"
    echo ""
    echo "To create the release, run:"
    echo "  gh release create $VERSION --prerelease --title='$TITLE' \\"
    echo "    --notes=\"\$(cat $NOTES_FILE)\" '$ZIP_FILE'"
    exit 1
fi

# Step 4: Upload complete package to existing release
echo ""
echo "Step 4️⃣  Uploading complete package to GitHub release..."
gh release upload "$VERSION" "$ZIP_FILE" --clobber

# Step 5: Success message
echo ""
echo "✅ Complete release package successfully uploaded!"
echo ""
echo "Release: https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/releases/tag/$VERSION"
