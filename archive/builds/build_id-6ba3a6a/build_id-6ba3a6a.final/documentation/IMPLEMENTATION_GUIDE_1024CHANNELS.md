# 1024-Channel Support Implementation Guide

## Overview
Fix channel duplication and enable full 1024-channel support with proper name storage.

## Files Created
1. `Drivers/n7six.ApeX.chirp.v7.6.9.1024ch_fix.patch` - CHIRP driver updates
2. `App/settings.1024ch_fix.patch` - Firmware updates

## Quick Start

### 1. Apply Firmware Patch
```bash
cd /path/to/repo
git apply App/settings.1024ch_fix.patch
./compile-with-docker.sh ApeX
```

Flash the new firmware using UVTools2.

### 2. Update CHIRP Driver
```bash
# Windows
cd "C:\Program Files\CHIRP\drivers"
git apply Drivers/n7six.ApeX.chirp.v7.6.9.1024ch_fix.patch

# Linux
cd ~/.local/share/chirp/drivers/
git apply Drivers/n7six.ApeX.chirp.v7.6.9.1024ch_fix.patch
```

Restart CHIRP.

### 3. Test
- Factory reset radio
- Save channels 131, 501 with names
- Download with CHIRP
- Verify no duplicates at channels 21-25 or 11-13

## What Was Fixed

**Firmware (App/settings.c):**
- Modified SETTINGS_SaveChannel() to save names for ALL 1024 channels
- Removed conditional that only saved names for channels 1-250

**CHIRP Driver:**
- Added firmware mirroring detection
- Added name fallback for channels > 250
- Added channel validation

## EEPROM Capacity
- Frequencies: 16KB / 16 bytes = 1024 channels
- Names: 16KB / 16 bytes = 1024 names
- Hardware supports 1024 channels, firmware just needs to use it

## Rollback
Keep backup of original driver and firmware to rollback if needed.

## Support
See patch files for detailed changes.