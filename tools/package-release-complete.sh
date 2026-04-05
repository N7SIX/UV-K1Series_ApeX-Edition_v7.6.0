#!/bin/bash
#
# =====================================================================================
# Comprehensive Release Package Creator
# Packages firmware binaries + documentation into a complete release ZIP
# Usage: ./tools/package-release-complete.sh [version] [output-dir]
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

# Build directory
BUILD_DIR="${REPO_ROOT}/build/ApeX"

# Validate all required files
if [[ -z "$LATEST_RELEASE_NOTE" ]]; then
    echo "ERROR: No release note found for version ${VERSION}" >&2
    exit 1
fi

RELEASE_NOTE_NAME="$(basename "$LATEST_RELEASE_NOTE")"

for file in "$MANUAL_FILE" "$SPECTRUM_GUIDE"; do
    if [[ ! -f "$file" ]]; then
        echo "ERROR: File not found: $file" >&2
        exit 1
    fi
done

# Check for build artifacts
BUILD_FILES=(
    "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.bin"
    "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.hex"
    "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.elf"
    "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.map"
)

for file in "${BUILD_FILES[@]}"; do
    if [[ ! -f "$file" ]]; then
        echo "ERROR: Build artifact not found: $file" >&2
        exit 1
    fi
done

# Create temporary directory for packaging
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Create release directory inside temp
RELEASE_DIR="${TEMP_DIR}/ApeX-${VERSION}"
mkdir -p "$RELEASE_DIR/Firmware" "$RELEASE_DIR/Documentation"

# Copy firmware files
cp "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.bin" "$RELEASE_DIR/Firmware/"
cp "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.hex" "$RELEASE_DIR/Firmware/"
cp "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.elf" "$RELEASE_DIR/Firmware/"
cp "${BUILD_DIR}/n7six.ApeX-k1.${VERSION}.map" "$RELEASE_DIR/Firmware/"

# Copy documentation files
cp "$MANUAL_FILE" "$RELEASE_DIR/Documentation/"
cp "$SPECTRUM_GUIDE" "$RELEASE_DIR/Documentation/"
cp "$LATEST_RELEASE_NOTE" "$RELEASE_DIR/Documentation/"

# Create manifest file
cat > "$RELEASE_DIR/MANIFEST.txt" << MANIFEST_EOF
===============================================================================
ApeX Edition ${VERSION} Complete Release Package
===============================================================================

CONTENTS:

├── Firmware/
│   ├── n7six.ApeX-k1.${VERSION}.bin    [86 KB]  - Binary firmware for flashing
│   ├── n7six.ApeX-k1.${VERSION}.hex    [240 KB] - Intel HEX format
│   ├── n7six.ApeX-k1.${VERSION}.elf    [149 KB] - ELF executable (for debugging)
│   └── n7six.ApeX-k1.${VERSION}.map    [296 KB] - Linker map (symbols/analysis)
│
├── Documentation/
│   ├── Owner's Manual - ApeX Edition.md       - Complete user manual
│   ├── SPECTRUM_ANALYZER_GUIDE.md             - Spectrum analyzer reference (v2.0)
│   └── ${RELEASE_NOTE_NAME}                    - Version-specific release notes
│
├── MANIFEST.txt                                - This file
└── FLASHING_INSTRUCTIONS.txt                   - Installation guide

===============================================================================
QUICK START

1. Choose your flashing method:
  - Multi-UVTools (recommended)
  - UVTools2 (web browser)
  - Use the .bin firmware image

2. Recommended: Use .bin file with Multi-UVTools or UVTools2

3. For detailed instructions, see FLASHING_INSTRUCTIONS.txt

===============================================================================
FILE DESCRIPTIONS

FIRMWARE:

n7six.ApeX-k1.${VERSION}.bin (86 KB)
  - Optimized binary firmware
  - Flash directly to 0x00000000
  - RECOMMENDED for most users
  - Compatible: UV-K1, UV-K5 V3 (PY32F071 only)

n7six.ApeX-k1.${VERSION}.hex (240 KB)
  - Intel HEX format (human-readable)
  - Compatible with general-purpose programmers
  - Same content as .bin, different format
  - Use if your programmer requires .hex format

n7six.ApeX-k1.${VERSION}.elf (149 KB)
  - ELF executable with debug symbols
  - For GDB debugging and analysis
  - NOT for flashing directly
  - Contains symbol table for function names

n7six.ApeX-k1.${VERSION}.map (296 KB)
  - Linker map file
  - Shows memory layout and symbol addresses
  - For engineers analyzing code/performance
  - NOT for flashing

DOCUMENTATION:

Owner's Manual - ApeX Edition.md
  - Complete user guide
  - All features and operations
  - Menu structure and settings
  - Recommended for first-time users

SPECTRUM_ANALYZER_GUIDE.md (v2.0)
  - Professional spectrum analyzer reference
  - Display interpretation
  - Analysis techniques
  - Register inspector guide (STILL mode)
  - Troubleshooting

${RELEASE_NOTE_NAME}
  - Version-specific release notes
  - Bug fixes in this version
  - Build metrics
  - Compatibility information

===============================================================================
HARDWARE REQUIREMENTS

✅ SUPPORTED:
  - UV-K1 Radio
  - UV-K5 V3 (firmware version 3.0+)
  - MCU: PY32F071

❌ NOT SUPPORTED:
  - UV-K5 V1 or V2 (different MCU)
  - UV-K5 V3 with older firmware

===============================================================================
BEFORE FLASHING

1. Back up your current radio configuration
2. Battery should be fully charged or connected to power supply
3. Prepare Multi-UVTools or UVTools2 before flashing
4. Review FLASHING_INSTRUCTIONS.txt for your specific radio model

===============================================================================
BUILD INFORMATION

Build Command:     ./compile-with-docker.sh ApeX
Build Status:      ✅ Success (83/83 objects)
Compiler:          arm-none-eabi-gcc 13.3.1
Target MCU:        PY32F071
RAM Usage:         14,400 B / 16 KB (87.89%)
FLASH Usage:       86,744 B / 118 KB (71.79%)
Build Date:        April 5, 2026
Firmware Version:  ${VERSION} (Patch Release)

===============================================================================
RELEASE NOTES SUMMARY

This patch resolves all known bugs in the spectrum analyzer STILL mode:
- RSSI dBm reading corrected (was showing 32000+ values)
- Helicopter audio noise eliminated
- PTT no-signal feedback overlay added
- Register inspector arrow-key multi-change bug fixed
- BPF selector fixed and persistent
- Rotary navigation for all register fields

See the included version-matching release note for complete details.

===============================================================================
SUPPORT & DOCUMENTATION

- GitHub: https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0
- Issues: https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/issues
- Releases: https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/releases

===============================================================================
LICENSE

Apache License, Version 2.0
See repository for full license text.

===============================================================================
MANIFEST_EOF

# Create flashing instructions
cat > "$RELEASE_DIR/FLASHING_INSTRUCTIONS.txt" << FLASHING_EOF
===============================================================================
FLASHING FIRMWARE TO UV-K1 / UV-K5 V3
===============================================================================

RECOMMENDED FLASHING METHODS (ONLY):

1. Multi-UVTools (recommended desktop method)
2. UVTools2 (recommended web method)

Before flashing:
1. Back up calibration/settings
2. Fully charge battery or use stable external power
3. Use firmware file: Firmware/n7six.ApeX-k1.${VERSION}.bin

METHOD 1: Multi-UVTools

1. Open Multi-UVTools
2. Select flash/update mode
3. Choose file: Firmware/n7six.ApeX-k1.${VERSION}.bin
4. Start flash and wait for completion

METHOD 2: UVTools2

1. Open UVTools2 in browser: https://armel.github.io/uvtools2/?mode=flash
2. Connect radio in flash mode
3. Select file: Firmware/n7six.ApeX-k1.${VERSION}.bin
4. Start flash and wait for completion

VERIFICATION:

After flashing:
1. Disconnect programmer
2. Power on radio
3. Check firmware version in Menu
4. Should show: N7SIX ${VERSION}

TROUBLESHOOTING:

Problem: Cannot connect to radio
  → Verify USB cable is connected
  → Reconnect radio and retry in Multi-UVTools/UVTools2
  → Close other serial/web-serial apps that may hold the port

Problem: Flashing fails partway through
  → Check power supply (radio should stay powered)
  → Retry using the .bin file
  → Check for corrupted .bin file (compare file size)

Problem: Radio won't boot after flashing
  → Verify correct firmware file was used
  → Try power cycle (remove battery, replace)
  → Radio may need recovery via stock firmware

For more help:
  → See Documentation/Owner's Manual - ApeX Edition.md
  → Check GitHub issues: https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/issues

===============================================================================
FLASHING_EOF

# Create zip file
ZIP_FILE="${OUTPUT_DIR}/ApeX-${VERSION}-Complete-Release.zip"
cd "$TEMP_DIR"
zip -r "$ZIP_FILE" "ApeX-${VERSION}" > /dev/null 2>&1

# Display result
echo "✅ Complete release package created:"
echo ""
echo "  File: $ZIP_FILE"
echo "  Size: $(du -h "$ZIP_FILE" | cut -f1)"
echo ""
echo "  CONTENTS:"
echo "  ├── Firmware/"
echo "  │   ├── n7six.ApeX-k1.${VERSION}.bin      (86 KB)   ← FLASH THIS"
echo "  │   ├── n7six.ApeX-k1.${VERSION}.hex      (240 KB)"
echo "  │   ├── n7six.ApeX-k1.${VERSION}.elf      (149 KB)"
echo "  │   └── n7six.ApeX-k1.${VERSION}.map      (296 KB)"
echo "  ├── Documentation/"
echo "  │   ├── Owner's Manual - ApeX Edition.md"
echo "  │   ├── SPECTRUM_ANALYZER_GUIDE.md"
echo "  │   └── ${RELEASE_NOTE_NAME}"
echo "  ├── MANIFEST.txt                         (This file listing)"
echo "  └── FLASHING_INSTRUCTIONS.txt            (Installation guide)"
echo ""
echo "  Ready for GitHub release attachment."
