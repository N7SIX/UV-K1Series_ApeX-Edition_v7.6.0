**Firmware Version:** v7.6.6-EDIT-THIS  
**Build Identifier:** n7six.ApeX-k1.v7.6.6-EDIT-THIS  
**Hardware:** UV-K1 / UV-K5 V3 (PY32F071 MCU)  
**Status:** [Pre-release / Release]

## Release Summary

[Insert concise summary of the release purpose and scope — 2-3 sentences]

### Key Fixes / Features

[List changes in numbered format or bullet points]

1. **Feature/Fix 1** — Brief description with technical context
2. **Feature/Fix 2** — Brief description with technical context
3. **Feature/Fix 3** — Brief description with technical context

### Build Information

| Metric | Value |
|--------|-------|
| Build Command | `./compile-with-docker.sh ApeX` |
| Build Status | ✅ Success (XX/XX objects) |
| Compiler | arm-none-eabi-gcc 13.3.1 |
| Target MCU | PY32F071 |
| RAM Usage | X,XXX B / 16 KB (X.XX%) |
| FLASH Usage | XXX,XXX B / 118 KB (XX.XX%) |

### Affected Files

- `App/path/affected-file.c` — Description of changes
- `App/path/another-file.h` — Description of changes

### Documentation

- [Spectrum Analyzer User Guide v2.0](https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/blob/main/Documentation/SPECTRUM_ANALYZER_GUIDE.md)
- [Release Notes](https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/blob/main/Documentation/release-notes/vX.X.X-apex.md)
- [Update Summary](https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/blob/main/Documentation/vX.X.X_UPDATE_SUMMARY.md)

### Compatibility

✅ UV-K1 and UV-K5 V3 with PY32F071 MCU  
❌ UV-K5 V1 / other MCUs not supported

### Upgrade Notes

- [Brief note 1 about changes/requirements]
- [Brief note 2 about backwards compatibility]
- [Any specific installation instructions if needed]

---

## Usage Instructions

1. Edit the version numbers (replace all X.X.X and buildidentifiers)
2. Update the Release Summary (2-3 sentences describing the release)
3. List Key Fixes / Features in numbered format
4. Update Build Information metrics (build the firmware and verify numbers)
5. List all modified files with brief descriptions
6. Update documentation links as needed
7. Keep compatibility section unchanged unless adding new MCU support
8. Add release-specific upgrade notes
9. Remove this "Usage Instructions" section before publishing

## For GitHub Release

Use with: `gh release edit <VERSION> --notes-file=release_notes.txt`

No main header needed — GitHub uses the release tag as the title automatically.
