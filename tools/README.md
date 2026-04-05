# Tools Directory

This directory contains utility scripts and tools for building, testing, and releasing the ApeX Edition firmware.

## Release Packaging Tools

### Standard Complete Release Package

All GitHub releases use one standardized complete package containing firmware artifacts and documentation.

**Scripts:**
- `package-release-complete.sh` — Packages firmware build outputs + docs into one ZIP
- `quick-release.sh` — Helper script to package and upload the complete ZIP to an existing GitHub release

#### Quick Start

```bash
# Create complete package for a release
./tools/package-release-complete.sh v7.6.5br3 build/

# Output: build/ApeX-v7.6.5br3-Complete-Release.zip
# Contains:
#   - Firmware: .bin, .hex, .elf, .map
#   - Owner's Manual - ApeX Edition.md
#   - SPECTRUM_ANALYZER_GUIDE.md
#   - v7.6.5br3-patch-apex.md (latest release note)
```

#### Documentation

See [RELEASE_PACKAGING_STANDARD.md](RELEASE_PACKAGING_STANDARD.md) for complete details on:
- Automated release packaging
- File requirements and structure
- GitHub release workflow
- Troubleshooting and best practices

---

## Existing Tools

### Build & Compilation

- `fw-pack.py` — Firmware packager (binary post-processing)

### Utilities

- `extract_bitmaps_to_png.py` — Extract LCD bitmap resources
- `k5viewer/` — K5 viewer/analyzer utilities
- `serialtool/` — Serial communication tools
- `misc/` — Miscellaneous utilities
- `unbrick_k5_v1/` — Recovery tools (UV-K5 V1)

---

## Release Workflow (Standard)

```
1. Update code and documentation
2. Create/update release notes in Documentation/release-notes/
3. Package release: ./tools/package-release-complete.sh v<VERSION> build/
4. Create GitHub release (with release notes)
5. Attach complete ZIP file
6. Publish release
```

Alternative: Use the quick helper:
```bash
./tools/quick-release.sh v7.6.5br3 "Release Title" release-notes.txt
```

---

## Notes

- All shell scripts require bash 4.0+
- Scripts use `set -e` for strict error handling
- Output paths support both relative and absolute directories
- Verify all documentation files exist before packaging

---

**Last Updated:** April 5, 2026  
**Maintainer:** N7SIX, Sean
