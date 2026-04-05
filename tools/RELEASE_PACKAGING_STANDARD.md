<!--
=====================================================================================
Release Packaging Standard (Complete ZIP)
Author: N7SIX, Sean
Version: 2.0
Date: April 5, 2026
=====================================================================================
-->

# ApeX Edition Release Packaging Standard

## Overview

Each release must publish a single complete ZIP package.
Do not publish a separate documentation-only ZIP.

## Required Release Asset

- `ApeX-v<VERSION>-Complete-Release.zip`

## Required ZIP Contents

1. Firmware files:
- `n7six.ApeX-k1.v<VERSION>.bin`
- `n7six.ApeX-k1.v<VERSION>.hex`
- `n7six.ApeX-k1.v<VERSION>.elf`
- `n7six.ApeX-k1.v<VERSION>.map`

2. Documentation files:
- `Owner's Manual - ApeX Edition.md`
- `SPECTRUM_ANALYZER_GUIDE.md`
- Latest version-matching release note in `Documentation/release-notes/`

3. Package metadata:
- `MANIFEST.txt`
- `FLASHING_INSTRUCTIONS.txt`

## Flashing Methods Policy

Only these flashing methods are documented/supported in package instructions:

1. Multi-UVTools
2. UVTools2

Do not include STM32CubeProgrammer/openocd/manual programmer workflows in release package instructions.

## Standard Workflow

```bash
# 1) Build
./compile-with-docker.sh ApeX

# 2) Package complete release
./tools/package-release-complete.sh v<VERSION> build/

# 3) Verify content
unzip -l build/ApeX-v<VERSION>-Complete-Release.zip

# 4) Attach to release
gh release upload v<VERSION> build/ApeX-v<VERSION>-Complete-Release.zip --clobber
```

## Asset Policy

- Keep exactly one ZIP package standard: complete-release ZIP.
- Remove old `ApeX-v<VERSION>_Documentation.zip` assets from releases.

## Validation Checklist

- Build succeeded
- Complete ZIP generated
- ZIP contains all required firmware and documentation files
- Release notes contain no private-repo doc links when docs are already in ZIP
- Release includes only complete ZIP (no docs-only ZIP)

## Changelog

- v2.0 (2026-04-05): switched from docs-only ZIP to complete-release ZIP standard; flashing instructions restricted to Multi-UVTools and UVTools2.
