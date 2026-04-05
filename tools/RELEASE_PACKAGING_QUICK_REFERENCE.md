# ApeX Release Documentation — Quick Reference

## Release Packaging in 3 Steps

### Step 1: Package Documentation
```bash
./tools/package-release-docs.sh v7.6.5br3 build/
```
✓ Creates: `build/ApeX-v7.6.5br3_Documentation.zip` (24KB)

### Step 2: Create Release on GitHub
```bash
gh release create v7.6.5br3 \
  --prerelease \
  --title="v7.6.5br3 Patch - Spectrum Analyzer STILL Mode Fixes" \
  --notes-file=release-notes.txt
```

### Step 3: Attach Documentation
```bash
gh release upload v7.6.5br3 build/ApeX-v7.6.5br3_Documentation.zip
```

---

## Alternative: One-Command Upload

If release already exists:
```bash
./tools/package-release-docs.sh v7.6.5br3 build/
gh release upload v7.6.5br3 build/ApeX-v7.6.5br3_Documentation.zip --clobber
```

---

## Files Included in Every Release ZIP

| File | Purpose | Size |
|------|---------|------|
| Owner's Manual - ApeX Edition.md | Complete user manual | ~18 KB |
| SPECTRUM_ANALYZER_GUIDE.md | Spectrum analyzer reference | ~32 KB |
| v7.6.5br3-patch-apex.md | Release-specific notes | ~4 KB |

---

## Before Releasing

✓ **Checklist:**

- [ ] All code changes complete and tested
- [ ] `Documentation/SPECTRUM_ANALYZER_GUIDE.md` updated (if spectrum changes)
- [ ] `Documentation/Owner's Manual - ApeX Edition.md` updated (if UI/features change)
- [ ] Release note created: `Documentation/release-notes/v<VERSION>*-apex.md`
- [ ] Build verified: `./compile-with-docker.sh ApeX` (clean 83/83)
- [ ] Documentation package created and verified

### Verify Package Before Release

```bash
./tools/package-release-docs.sh v7.6.5br3 build/
unzip -l build/ApeX-v7.6.5br3_Documentation.zip
ls -lh build/ApeX-v7.6.5br3_Documentation.zip
```

---

## GitHub Release URL Template

After publishing:
```
https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/releases/tag/v7.6.5br3
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "No release note found" | Create `Documentation/release-notes/v7.6.5br3-*-apex.md` |
| "Owner's Manual not found" | Check exact filename: `Owner's Manual - ApeX Edition.md` |
| ZIP file missing files | Run: `unzip -l build/ApeX-*.zip` to verify contents |
| Authentication error with `gh` | Run: `gh auth login` to re-authenticate |

---

## Standard ZIP Structure

```
ApeX-v7.6.5br3_Documentation/
├── Owner's Manual - ApeX Edition.md
├── SPECTRUM_ANALYZER_GUIDE.md
└── v7.6.5br3-patch-apex.md
```

Users extract this folder and reference markdown files locally.

---

**Documentation:** tools/RELEASE_PACKAGING_QUICK_REFERENCE.md  
**Valid from:** April 5, 2026 onwards
