<!--
=====================================================================================
Release Documentation Packaging Standard
Author: N7SIX, Sean
Version: 1.0
Date: April 5, 2026
=====================================================================================
-->

# ApeX Edition Release Documentation Packaging Standard

## Overview

All GitHub releases for the ApeX Edition now include a standardized documentation package containing the three essential user-facing guides. This ensures users downloading a release have immediate access to complete, version-matched documentation.

## Standard Documentation Package Contents

Every release ZIP attachment includes:

1. **Owner's Manual - ApeX Edition.md** — Complete user manual for all features
2. **SPECTRUM_ANALYZER_GUIDE.md** — Detailed spectrum analyzer operation and reference
3. **Latest Release Note** — Version-specific release notes (e.g., `v7.6.5br3-patch-apex.md`)

### File Structure in ZIP

```
ApeX-v7.6.5br3_Documentation/
├── Owner's Manual - ApeX Edition.md
├── SPECTRUM_ANALYZER_GUIDE.md
└── v7.6.5br3-patch-apex.md
```

## Creating a Release with Documentation Package

### Automated Method (Recommended)

```bash
# 1. Navigate to repository root
cd /workspaces/UV-K1Series_ApeX-Edition_v7.6.0

# 2. Create documentation package for specific version
./tools/package-release-docs.sh v7.6.5br3 build/

# 3. This generates: build/ApeX-v7.6.5br3_Documentation.zip

# 4. Create GitHub release via gh CLI
gh release create v7.6.5br3 \
  --prerelease \
  --title="v7.6.5br3 Patch - Spectrum Analyzer STILL Mode Fixes" \
  --notes-file=your_release_notes.txt \
  build/ApeX-v7.6.5br3_Documentation.zip

# Or manually:
gh release upload v7.6.5br3 build/ApeX-v7.6.5br3_Documentation.zip
```

### Manual Method

1. Run the packaging script:
   ```bash
   ./tools/package-release-docs.sh <VERSION> build/
   ```
   - `<VERSION>` — version string like `v7.6.5br3`
   - Output location defaults to `build/` directory

2. The script automatically:
   - Finds the latest release note for that version
   - Verifies all required files exist
   - Creates a properly structured ZIP
   - Reports size and contents

3. Upload the ZIP to GitHub release as an asset

## Script Usage Reference

### Command Syntax
```bash
./tools/package-release-docs.sh <VERSION> [OUTPUT_DIR]
```

### Parameters
- `<VERSION>` (required) — Release version (e.g., `v7.6.5br3`, `v7.6.6`)
- `[OUTPUT_DIR]` (optional) — Output directory for ZIP file (default: current directory)

### Examples

```bash
# Create package in build/ directory
./tools/package-release-docs.sh v7.6.5br3 build/

# Create package in current directory
./tools/package-release-docs.sh v7.6.5br3

# Create package for v7.6.6
./tools/package-release-docs.sh v7.6.6 /tmp/releases/
```

### Output

The script generates a ZIP file named: `ApeX-<VERSION>_Documentation.zip`

Example:
```
✓ Release documentation package created:
  File: /workspaces/.../build/ApeX-v7.6.5br3_Documentation.zip
  Size: 24K

  Contents:
  - Owner's Manual - ApeX Edition.md
  - SPECTRUM_ANALYZER_GUIDE.md
  - v7.6.5br3-patch-apex.md

Ready for GitHub release attachment.
```

## Release Note File Matching

The script automatically finds the **latest version-matching release note** in `Documentation/release-notes/`:

- For version `v7.6.5br3`:
  - Searches for: `v7.6.5br3*-apex.md`
  - Matches: `v7.6.5br3-patch-apex.md` ✓
  - Matches: `v7.6.5br3-apex.md` (if exists)
  - Takes the **latest** alphabetically if multiple exist

## File Requirements

The script validates that all required source files exist:

```
Documentation/
├── Owner's Manual - ApeX Edition.md    ← Required
├── SPECTRUM_ANALYZER_GUIDE.md          ← Required
└── release-notes/
    └── v7.6.5br3-*-apex.md            ← Must exist
```

If any file is missing, the script exits with an error message indicating which file cannot be found.

## Best Practices

### Before Creating a Release

1. **Update all documentation** — Ensure all three files are current for the release
2. **Verify release note exists** — Create `Documentation/release-notes/v<VERSION>-apex.md` or `v<VERSION>-patch-apex.md`
3. **Test the packaging script** before attaching to release:
   ```bash
   ./tools/package-release-docs.sh v7.6.5br3 /tmp/test/
   unzip -l /tmp/test/ApeX-v7.6.5br3_Documentation.zip
   ```

### Release Workflow

```
1. [ ] Update code / features
2. [ ] Update SPECTRUM_ANALYZER_GUIDE.md (if spectrum changes)
3. [ ] Update Owner's Manual (if UI/feature changes)
4. [ ] Create/update release note in Documentation/release-notes/
5. [ ] Run: ./tools/package-release-docs.sh v<VERSION> build/
6. [ ] Verify: unzip -l build/ApeX-v<VERSION>_Documentation.zip
7. [ ] Create gh release
8. [ ] Upload ZIP as asset or attach during release creation
9. [ ] Publish release
```

### Checking Documentation Freshness

Before creating a release, verify documentation versions match:

```bash
# Show version references in guides
grep -h "Firmware.*:" Documentation/{Owner's\ Manual,SPECTRUM_ANALYZER_GUIDE}.md \
  Documentation/release-notes/v7.6.5br3*.md | head -5
```

## Changelog / Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-04-05 | Initial release packaging standard |

## Troubleshooting

### Script says "No release note found"

**Problem:** The script cannot find a release note matching the version.

**Solution:** Create the appropriate release note file:
```bash
# Copy from template
cp Documentation/release-notes/RELEASE_NOTE_TEMPLATE.md \
   Documentation/release-notes/v7.6.5br3-patch-apex.md
# Edit with version-specific content
```

### ZIP file too small or missing files

**Problem:** Zip contains fewer than 4 items or is < 20K.

**Solution:** Verify source files exist and are readable:
```bash
ls -la Documentation/{Owner\'s\ Manual,SPECTRUM_ANALYZER_GUIDE}.md
ls -la Documentation/release-notes/v7.6.5br3*.md
```

### "Owner's Manual not found" error

**Problem:** Filename has subtle differences (spacing, capitalization, etc.).

**Solution:** Check exact filename:
```bash
ls -la Documentation/ | grep -i manual
```

The filename **must be exactly**: `Owner's Manual - ApeX Edition.md`

## Future Enhancements

Potential improvements to this system:

- [ ] Automated CI/CD integration to generate packages on tag creation
- [ ] Multi-format releases (PDF, EPUB in addition to Markdown)
- [ ] Language-specific documentation packages
- [ ] Automated changelog generation from commit history
- [ ] Documentation version validation in release pipeline

---

**Documentation:** tools/RELEASE_PACKAGING_STANDARD.md  
**Maintainer:** N7SIX, Sean  
**License:** Apache License, Version 2.0
