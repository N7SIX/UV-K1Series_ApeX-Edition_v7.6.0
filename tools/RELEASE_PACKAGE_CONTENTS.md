<!--
=====================================================================================
v7.6.5br3 Complete Release Package — Contents & Instructions
Author: N7SIX, Sean
Date: April 5, 2026
=====================================================================================
-->

# ApeX Edition v7.6.5br3 Complete Release Package

## 📦 What's Inside

**File:** `ApeX-v7.6.5br3-Complete-Release.zip` (304 KB)

### Directory Structure

```
ApeX-v7.6.5br3/
├── Firmware/                                  [671 KB total]
│   ├── n7six.ApeX-k1.v7.6.5br3.bin           [87 KB]   ← FLASH THIS
│   ├── n7six.ApeX-k1.v7.6.5br3.hex           [245 KB]
│   ├── n7six.ApeX-k1.v7.6.5br3.elf           [153 KB]
│   └── n7six.ApeX-k1.v7.6.5br3.map           [302 KB]
│
├── Documentation/                             [54 KB total]
│   ├── Owner's Manual - ApeX Edition.md       [18 KB]
│   ├── SPECTRUM_ANALYZER_GUIDE.md             [32 KB]
│   └── v7.6.5br3-patch-apex.md                [4 KB]
│
├── MANIFEST.txt                               [5 KB]  - Complete file listing
└── FLASHING_INSTRUCTIONS.txt                  [2 KB]  - Installation guide
```

---

## 🔧 Firmware Files Explained

| File | Size | Purpose | For Whom |
|------|------|---------|----------|
| **n7six.ApeX-k1.v7.6.5br3.bin** | 87 KB | Binary firmware - **RECOMMENDED** | Most users |
| n7six.ApeX-k1.v7.6.5br3.hex | 245 KB | Intel HEX format (alternative) | Hex programmer users |
| n7six.ApeX-k1.v7.6.5br3.elf | 153 KB | ELF with debug symbols (NOT for flashing) | Developers/debuggers |
| n7six.ApeX-k1.v7.6.5br3.map | 302 KB | Linker map (NOT for flashing) | Engineers/analysis |

### Which File Should I Flash?

**→ Use: `n7six.ApeX-k1.v7.6.5br3.bin`** (87 KB)

This is the compiled firmware ready to flash to your radio.

---

## 📚 Documentation Files

### 1. Owner's Manual - ApeX Edition.md (18 KB)
- **What:** Complete user manual for all ApeX features
- **When to read:** First time setup, learning new features
- **Contains:** Menu structure, all settings, troubleshooting

### 2. SPECTRUM_ANALYZER_GUIDE.md (32 KB)
- **What:** Professional spectrum analyzer reference (Version 2.0)
- **When to read:** Using spectrum analysis features
- **Contains:** 
  - Display interpretation
  - STILL mode register inspector guide
  - Analysis techniques
  - Troubleshooting for spectrum issues

### 3. v7.6.5br3-patch-apex.md (4 KB)
- **What:** Release-specific notes for this version
- **When to read:** Before upgrading
- **Contains:**
  - Bug fixes in v7.6.5br3
  - Build metrics
  - Compatibility information
  - Upgrade notes

---

## 🚀 Quick Start — 3 Steps

### Step 1: Extract ZIP
```bash
unzip ApeX-v7.6.5br3-Complete-Release.zip
cd ApeX-v7.6.5br3
```

### Step 2: Read Flashing Instructions
```bash
cat FLASHING_INSTRUCTIONS.txt
```

### Step 3: Flash the Firmware
Use STM32CubeProgrammer or your preferred tool with:
```
File: Firmware/n7six.ApeX-k1.v7.6.5br3.bin
Target Address: 0x00000000
```

See `FLASHING_INSTRUCTIONS.txt` for detailed steps.

---

## 📋 Individual File Info

### Firmware/n7six.ApeX-k1.v7.6.5br3.bin
```
Size:        87,184 bytes (87 KB)
Format:      Binary (raw bytes)
Flash to:    Address 0x00000000
Checksum:    [Generated during build]
Supported:   UV-K1, UV-K5 V3 (PY32F071 only)
```

**How to use:**
1. STM32CubeProgrammer: File → Open File → select .bin
2. openocd: `flash write_bank 0 <file.bin> 0`
3. Generic programmer: Select binary mode, address 0x00000000

### Firmware/n7six.ApeX-k1.v7.6.5br3.hex
```
Size:        245,299 bytes (240 KB)
Format:      Intel Hexadecimal
Use when:    Your programmer only accepts .hex format
Equivalent:  Same content as .bin, different format
```

### Firmware/n7six.ApeX-k1.v7.6.5br3.elf
```
Size:        152,564 bytes (149 KB)
Format:      ELF Executable (with debug symbols)
Use for:     GDB debugging, symbol analysis
DO NOT FLASH: This is not a flashable format
Contains:    Function names, variable locations, line numbers
```

### Firmware/n7six.ApeX-k1.v7.6.5br3.map
```
Size:        302,398 bytes (296 KB)
Type:        Linker map file (text format)
Use for:     Memory layout analysis, optimization
DO NOT FLASH: This is a reference document
Shows:       All symbols, addresses, section sizes
```

---

## ✅ What's Fixed in v7.6.5br3

1. **RSSI dBm Reading** — Fixed 32000+ dBm display artifacts
2. **Helicopter Audio** — Eliminated oscillator noise in STILL mode
3. **PTT No-Signal Feedback** — Added overlay when no signal detected
4. **Register Inspector** — Fixed arrow-down multi-field changes
5. **BPF Selector** — Now persistent and user-controllable
6. **Rotary Navigation** — All register fields wrap at both ends

See `Documentation/v7.6.5br3-patch-apex.md` for full details.

---

## 🔒 Hardware Requirements

### ✅ Supported
- UV-K1 Radio
- UV-K5 V3 (firmware v3.0+)
- MCU: PY32F071

### ❌ Not Supported
- UV-K5 V1 or V2 (different MCU)
- Other firmware variants

---

## 📖 Documentation Quick Links

| Doc | For | Read Time |
|-----|-----|-----------|
| Owner's Manual | General users | 30-45 min |
| SPECTRUM_ANALYZER_GUIDE | Spectrum users | 45-60 min |
| v7.6.5br3-patch-apex | All users | 5 min |
| FLASHING_INSTRUCTIONS | Before flashing | 10-15 min |
| MANIFEST.txt | Reference | 5 min |

---

## ❓ Common Questions

**Q: Which file do I need to flash?**  
A: Use `Firmware/n7six.ApeX-k1.v7.6.5br3.bin` (87 KB). Don't use .elf or .map files.

**Q: What's the difference between .bin and .hex?**  
A: Same firmware, different format. Use .bin if possible (smaller, faster). Use .hex if your programmer requires it.

**Q: Can I flash the .elf file?**  
A: No. The .elf file contains debug symbols and is for development/analysis only. It cannot be flashed directly.

**Q: Do I need to back up anything before flashing?**  
A: Yes, back up your settings/frequencies. The flashing process resets calibration data.

**Q: What if flashing fails?**  
A: See `FLASHING_INSTRUCTIONS.txt` troubleshooting section, or check GitHub issues.

**Q: Which radio version do I have?**  
A: Check radio model: UV-K1 or UV-K5 V3. This firmware is **NOT** for UV-K5 V1/V2.

---

## 📞 Support

- **GitHub Issues:** https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/issues
- **Releases:** https://github.com/N7SIX/UV-K1Series_ApeX-Edition_v7.6.0/releases
- **Documentation:** See included .md files

---

## 📝 Build & Verification

```
Build Command:   ./compile-with-docker.sh ApeX
Compiler:        arm-none-eabi-gcc 13.3.1
Target:          PY32F071 MCU
Build Status:    ✅ Success (83/83 objects compiled)
Flash Usage:     86,744 / 118,784 bytes (71.79%)
RAM Usage:       14,400 / 16,384 bytes (87.89%)
Build Date:      April 5, 2026
```

---

## 📄 License

Apache License, Version 2.0

See repository LICENSE file for full text.

---

**Package Created:** April 5, 2026  
**Firmware Version:** v7.6.5br3 (ApeX Edition)  
**Build ID:** n7six.ApeX-k1.v7.6.5br3  
**Status:** Pre-release (patch)
