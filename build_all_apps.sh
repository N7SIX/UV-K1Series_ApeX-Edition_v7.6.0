#!/usr/bin/env bash
# rebuild_all_apps.sh - Rebuild all overlay apps in App/apps/
set -euo pipefail

CDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$CDIR"

TOTAL=0
FAILED=0

mapfile -t APP_DIRS < <(powershell -NoProfile -Command '
Get-ChildItem "./App/apps" -Directory | ForEach-Object {
    $dir = $_.FullName
    $app = Split-Path $dir -Leaf
    $out = Join-Path $dir ($app + ".app" )
    if(Test-Path $out) {
        Write-Host "$app already built"
    } else {
        Write-Output $dir
    }
}
' | grep -v "already built" | sort)

echo "=============================================="
echo "  Rebuilding all overlay apps"
echo "  Found ${#APP_DIRS[@]} app directories"
echo "=============================================="
echo ""

for APP_DIR in "${APP_DIRS[@]}"; do
    cd "$APP_DIR"
    APP_NAME="$(basename "$PWD")"
    echo "  Building $APP_NAME..."

    if ./build.sh 2>&1; then
        echo "      ✅ Success (built: $(ls -1 *.app 2>/dev/null | head -1 || echo unknown))"
    else
        echo "      ❌ FAILED: $APP_NAME"
        FAILED=$((FAILED+1))
    fi
    TOTAL=$((TOTAL+1))
    cd "$CDIR"
    echo ""
done

echo "=============================================="
echo "  Total: $TOTAL, Failed: $FAILED"
echo "=============================================="

if [ "$FAILED" -gt 0 ]; then
    echo "⚠️  Some apps failed to build!"
    exit 1
else
    echo "🎉 All apps built successfully!"
    exit 0
fi