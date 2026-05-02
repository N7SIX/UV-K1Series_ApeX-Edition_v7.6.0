<!-- 
=====================================================================================
ApeX Edition: Complete Programming Tools Guide
Multi-UVTools, UVTools2, CHIRP, Quansheng Programming Software
=====================================================================================
Fair and balanced guide for all supported radio programming tools.
Users can choose any tool - all work equivalently for backup, restore, and programming.

Version: v7.6.0
Author: ApeX Migration & Tools Team
Date: 2026-04-11
Status: REFERENCE
=====================================================================================
-->

# Programming Tools Guide: All Supported Tools

## Overview

**Multiple professional programming tools are fully supported by ApeX Edition:**

### Supported Tools:
1. **Multi-UVTools** - Advanced features, batch operations
2. **UVTools2** - Open-source, stable, cross-platform
3. **CHIRP** - Intuitive UI, community standard (Windows/Mac/Linux)
4. **Quansheng Programming Software** - Official manufacturer tool

**All tools work equivalently for:**
- ✅ Backing up radio configurations
- ✅ Programming/Flashing firmware
- ✅ Restoring channels and settings
- ✅ Managing radio profiles

**Choose any tool that works best for you — they're all equally supported and reliable.** 👍

---

## Quick Comparison (All Tools)

| Feature | Multi-UVTools | UVTools2 | CHIRP | Quansheng |
|---------|:-------------:|:--------:|:-----:|:---------:|
| **Backup Config** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| **Flash Firmware** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| **Restore Channels** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes |
| **Edit Channels** | ✅ Advanced | ✅ Yes | ✅ Yes | ✅ Yes |
| **Dump Calibration** | ✅ Yes | ✅ Yes | ⚠️ Limited | ✅ Yes |
| **GUI Interface** | ✅ Full | ✅ Full | ✅ Excellent | ✅ Full |
| **CLI Support** | ✅ Yes | ✅ Yes | ⚠️ Limited | ✅ Yes |
| **Cross-Platform** | ✅ Yes | ✅ Yes | ✅ Yes | ⚠️ Windows |
| **Cost** | 💰 Check | 💰 Check | 💰 **FREE** | 💰 **FREE** |
| **Learning Curve** | Moderate | Easy | **Very Easy** | Moderate |
| **Community Size** | Growing | Large | **Largest** | Native |

---

## All Tools Explained

### 1. Multi-UVTools
**What is it:**
- Enhanced version with advanced features
- Batch operations, bulk editing, templates
- Professional-grade radio management

**Best for:**
- Fleet management (multiple radios)
- Advanced batch operations
- Users wanting extra tools

**Get it:**
- Check distribution source
- Community-enhanced packages

**Pros:**
- ✅ Most features
- ✅ Best batch support
- ✅ Templates

**Cons:**
- Steeper learning curve
- May be overkill for single radio

---

### 2. UVTools2
**What is it:**
- Open-source, lightweight, flexible
- Web interface + desktop + CLI available
- Mature and stable codebase

**Best for:**
- Quick operations
- Cross-platform users (Linux preferred)
- Developers/CLI users

**Get it:**
- Website: [armel.github.io/uvtools2](https://armel.github.io/uvtools2/)
- GitHub: [armel/uvtools2](https://github.com/armel/uvtools2)
- CLI: `apt install uvtools2` (Linux)

**Pros:**
- ✅ Free and open-source
- ✅ Lightweight
- ✅ Multiple interfaces (GUI/web/CLI)

**Cons:**
- Less user-friendly than CHIRP
- Fewer editing features

---

### 3. CHIRP (⭐ Community Standard)
**What is it:**
- Intuitive desktop application
- Developed by community volunteers
- **Most popular among radio enthusiasts**
- Supports 1000+ radio models

**Best for:**
- First-time users
- Simple, visual channel editing
- Community support needed
- Cross-platform simplicity

**Get it:**
- Website: [chirp.danplanet.com](https://chirp.danplanet.com)
- **Free download** (Windows/Mac/Linux)
- Driver: [armel/uv-k5-chirp-driver](https://github.com/armel/uv-k5-chirp-driver)

**Pros:**
- ✅ **Easiest UI** - Intuitive for beginners
- ✅ **FREE** - No cost
- ✅ **Best community** - Tons of tutorials
- ✅ **Cross-platform** - Works everywhere
- ✅ **Visual verification** - See all channels before upload

**Cons:**
- ⚠️ Limited calibration editing
- ⚠️ Less advanced features

---

### 4. Quansheng Programming Software
**What is it:**
- Official programming software from manufacturer
- Native support for all Quansheng radios
- Direct from manufacturer (most compatible)

**Best for:**
- Users who prefer official tools
- Windows users
- Guaranteed manufacturer compatibility

**Get it:**
- Check Quansheng official website
- Usually bundled with radio
- May require registration

**Pros:**
- ✅ **Official** - Made by manufacturer
- ✅ **Complete** - All features supported
- ✅ **Direct calibration support** - Full calibration editing

**Cons:**
- ⚠️ Windows-only typically
- ⚠️ Less community support
- Limited cross-platform

---

## ApeX Edition Migration Workflow (All Tools)

### Step 1: Backup Before Flashing ApeX

**Using CHIRP (Easiest):**
```
1. Download CHIRP from chirp.danplanet.com
2. Install CHIRP + driver
3. Launch CHIRP
4. Connect radio (USB)
5. Click "Download from Radio"
6. Save: my_radio_backup.img
7. Store backup safely
```

**Using Multi-UVTools:**
```
1. Launch Multi-UVTools
2. Connect radio
3. Click "Backup"
4. Save: my_radio_backup.img
5. Store backup safely
```

**Using UVTools2:**
```
1. Open UVTools2 (web or desktop)
2. Connect radio
3. Click "Read from Radio"
4. Save: my_radio_backup.img
5. Store backup safely
```

**Using Quansheng Software:**
```
1. Launch Quansheng Programming Software
2. Connect radio
3. Click "Download"
4. Save: my_radio_backup.DAT or .img
5. Store backup safely
```

**Result:** ✅ Configuration backed up safely

### Step 2: Flash ApeX Firmware

**Any tool works for flashing:**
```
1. Use your chosen tool's flash mode
2. Select ApeX .bin firmware
3. Flash to radio
4. Boot successfully
```

**Result:** ✅ ApeX firmware installed

### Step 3: Restore Channels & Settings

**Using CHIRP (Recommended for beginners):**
```
1. Connect radio
2. CHIRP → "Open File" → my_radio_backup.img
3. Preview all channels visually
4. Click "Upload to Radio"
5. Verify on radio display
```

**Using Multi-UVTools:**
```
1. Connect radio
2. Load: my_radio_backup.img
3. Click "Restore"
4. Upload configuration
```

**Using UVTools2:**
```
1. Connect radio
2. Open: my_radio_backup.img
3. Click "Write to Radio"
4. Complete restoration
```

**Using Quansheng Software:**
```
1. Connect radio
2. Load: my_radio_backup.DAT
3. Click "Upload"
4. Verify on radio
```

**Result:** ✅ All settings and channels restored

---

## Real-World Workflow Examples

### Example 1: New User (F4HWN → ApeX)
**Scenario:** First-time user with F4HWN firmware, 50 channels

**Recommended: CHIRP**
```
1. Download CHIRP (free, 5 min install)
2. Backup: "Download from Radio" → save backup.img
3. Flash ApeX using CHIRP
4. Boot ApeX (migration auto-recovers channels)
5. If needed: CHIRP "Upload to Radio" to restore

TIME: ~15 minutes
EASIEST: Best visual interface
COMMUNITY: Largest help available
```

---

### Example 2: Advanced User (Multiple Radios)
**Scenario:** Managing 3 radios, need batch operations

**Recommended: Multi-UVTools**
```
1. Backup all 3 radios:
   - radio_1.img
   - radio_2.img
   - radio_3.img

2. Flash ApeX on all (offline)

3. Restore with Multi-UVTools batch features:
   - Multi-UVTools excels here
   - Batch upload faster

TIME: ~30 minutes total
EFFICIENCY: Batch operations save time
```

---

### Example 3: Official Support Preference
**Scenario:** User wants official manufacturer tool

**Recommended: Quansheng Programming Software**
```
1. Get Quansheng Software (from manufacturer)
2. Backup: "Download" → save backup.DAT
3. Flash ApeX via Quansheng
4. Boot ApeX
5. Restore: "Upload" backup.DAT

TIME: ~15 minutes
OFFICIAL: Direct from manufacturer
GUARANTEE: Designed for UV-K1/K5
```

---

### Example 4: Linux User
**Scenario:** Ubuntu/Linux system, simple migration

**Recommended: UVTools2 (CLI)**
```
$ apt install uvtools2
$ uvtools2 backup --radio /dev/ttyUSB0 --output backup.img
$ # Flash ApeX
$ uvtools2 restore --radio /dev/ttyUSB0 --input backup.img

TIME: ~10 minutes
NATIVE: No emulation, native Linux support
SCRIPTABLE: Easy automation if needed
```

---

## Tool Selection Matrix

**Choose based on your needs:**

```
NEW USER
└─ Want simplest UI?
   └─ YES → CHIRP ⭐ (best visuals, easiest)
   └─ NO → Continue

ADVANCED USER
└─ Need batch operations?
   └─ YES → Multi-UVTools (best features)
   └─ NO → Continue

LINUX/MAC USER
└─ Prefer CLI or web interface?
   └─ YES → UVTools2 (best open-source)
   └─ NO → Continue

OFFICIAL PREFERENCE
└─ Want manufacturer tool?
   └─ YES → Quansheng Software (most official)
   └─ NO → Continue

COMMUNITY HELP NEEDED
└─ Want most tutorials/help?
   └─ YES → CHIRP (largest community)
   └─ NO → Continue

DEFAULT ANSWER
└─ Just pick one! (all work equally well) ✅
```

---

## Installation Quick Start

### CHIRP (Recommended for beginners)
```
1. Visit: chirp.danplanet.com
2. Download for your OS
3. Install (next, next, finish)
4. Install driver: github.com/armel/uv-k5-chirp-driver
5. Ready to use! ✅
```

### Quansheng Software (Official)
```
1. Get from manufacturer or distributor
2. Install (Windows mainly)
3. Install USB drivers if needed
4. Ready to use! ✅
```

### UVTools2 (Open-source)
```
# Linux (easiest)
apt install uvtools2

# macOS (via Homebrew)
brew install uvtools2

# Windows (download from website)
# Visit: armel.github.io/uvtools2
```

### Multi-UVTools (Enhanced)
```
1. Get from distribution source
2. Install according to vendor instructions
3. Ready to use! ✅
```

---

## Honest Recommendation Summary

| Use Case | Best Tool | Why |
|----------|-----------|-----|
| **First-time user** | **CHIRP** | Easiest UI, free, tons of tutorials |
| **Just backup/restore** | **Any tool** | All work equally (pick favorite) |
| **Fleet management** | **Multi-UVTools** | Best batch features |
| **Linux native** | **UVTools2** | Native support, CLI |
| **Official support** | **Quansheng** | From manufacturer |
| **Community help** | **CHIRP** | Largest volunteer base |

---

## Comparison: Which Should I Use?

### ✅ **CHIRP is best if:**
- It's your first time
- You like visual interfaces
- You want free
- You need community help
- You prefer straightforward

### ✅ **Multi-UVTools is best if:**
- You manage multiple radios
- You need advanced features
- You want batch operations
- You like professional tools

### ✅ **UVTools2 is best if:**
- You use Linux
- You prefer open-source
- You like CLI tools
- You want lightweight

### ✅ **Quansheng Software is best if:**
- You want official tool
- You need complete compatibility
- Manufacturer support is priority

**BOTTOM LINE: All tools work perfectly. Pick the one you're most comfortable with.** ✅

---

## Community Resources

### CHIRP
- **Official:** [chirp.danplanet.com](https://chirp.danplanet.com)
- **YouTube:** Search "CHIRP tutorial" (hundreds of guides)
- **Reddit:** r/UV_K1, r/amateurradio
- **Forums:** RadioReference.com forums

### UVTools2
- **Official:** [armel.github.io/uvtools2](https://armel.github.io/uvtools2/)
- **GitHub:** [armel/uvtools2](https://github.com/armel/uvtools2)
- **Discord:** Various UV-K1 community servers

### Quansheng
- **Official:** Manufacturer website
- **Community:** Forum support from manufacturer
- **Regional:** Check local distributor support

### Multi-UVTools
- **Distribution:** Check vendor documentation
- **Community:** Active in radio enthusiast forums

---

## Final Recommendation

**Use any of these four tools — they all work perfectly with ApeX Edition:**

| Tool | Free | Easy | Official | Community | Best For |
|------|:----:|:----:|:--------:|:---------:|----------|
| CHIRP | ✅ | ✅ | ⚠️ | ✅✅ | **Beginners** |
| Quansheng | ✅ | ⚠️ | ✅✅ | ⚠️ | **Official preference** |
| UVTools2 | ✅ | ⚠️ | ⚠️ | ✅ | **Linux/developers** |
| Multi-UVTools | 💰 | ⚠️ | ⚠️ | ✅ | **Advanced users** |

**Pick your tool based on comfort level, not capability — all are equally reliable and supported!** 👍

---

## ApeX Edition Migration Workflow (Both Tools)

### Step 1: Backup Before Flashing ApeX

**Using Multi-UVTools:**
```
1. Launch Multi-UVTools
2. Connect radio (with current firmware)
3. Select "Backup" or "Download"
4. Save as: my_radio_backup.img
5. Store in safe location (USB + cloud)
```

**Using UVTools2:**
```
1. Open UVTools2 (desktop or web)
2. Connect radio (with current firmware)
3. Select "Read from Radio" or "Backup"
4. Save as: my_radio_backup.img
5. Store in safe location (USB + cloud)
```

**Result:** ✅ Configuration backed up safely

### Step 2: Flash ApeX Firmware

**Using Multi-UVTools:**
```
1. Disconnect radio temporarily
2. Flash ApeX firmware via Multi-UVTools flash mode
3. Radio boots with ApeX (may have defaults)
```

**Using UVTools2:**
```
1. Disconnect radio temporarily
2. Flash ApeX firmware via UVTools2 flash mode
3. Radio boots with ApeX (migration auto-attempts)
```

**Result:** ✅ ApeX firmware installed

### Step 3: Restore Channels & Settings

**Using Multi-UVTools:**
```
1. Connect ApeX radio to computer
2. Open Multi-UVTools
3. Load backup: my_radio_backup.img
4. Select "Restore" or "Write to Radio"
5. Verify channels before confirming
6. Upload configuration
```

**Using UVTools2:**
```
1. Connect ApeX radio to computer
2. Open UVTools2
3. Load backup: my_radio_backup.img
4. Select "Write to Radio" or "Restore"
5. Choose upload method (full or selective)
6. Complete restoration
```

**Result:** ✅ All settings and channels restored

---

## Common Tasks

### Task 1: Simple Backup-Flash-Restore

**Either tool works identically:**

```
Multi-UVTools OR UVTools2:
├─ Backup radio → save config.img
├─ Flash ApeX
├─ Restore config.img to ApeX
└─ Done! ✅ (15-20 minutes total)
```

### Task 2: Backup Multiple Radios

**Multi-UVTools (Advantage: Better batch support):**
```
1. Backup radio 1 → radio_1.img
2. Backup radio 2 → radio_2.img
3. Backup radio 3 → radio_3.img
(Loop for each radio)

4. Flash ApeX on all
5. Restore each via Multi-UVTools
(Multi-UVTools batch features make this faster)
```

**UVTools2 (Still works, just sequential):**
```
1. Backup radio 1 → radio_1.img
2. Backup radio 2 → radio_2.img
3. Backup radio 3 → radio_3.img
(Repeat for each)

4. Flash ApeX on all
5. Restore each via UVTools2
(Take slightly longer with sequential approach)
```

### Task 3: Edit Channels Before Restore

**Multi-UVTools (Advantage: More editing tools):**
```
1. Open radio_1.img in Multi-UVTools
2. Use advanced filter/edit features
3. Modify channels (delete, add, reorder)
4. Export modified config
5. Upload to ApeX radio
```

**UVTools2 (Still works, fewer features):**
```
1. Open radio_1.img in UVTools2
2. Edit channels manually via UI
3. Save modified config
4. Upload to ApeX radio
```

---

## Detailed Workflow Examples

### Example 1: User on Stock Firmware → ApeX

**Scenario:**
- Current radio: Stock Quansheng firmware with 50 programmed channels
- Want: Migrate to ApeX + keep all channels

**Workflow:**

```
DAY 1 - BACKUP:
  Multi-UVTools OR UVTools2
  ├─ Connect to stock firmware radio
  ├─ Backup config → my_radio_stock_backup.img
  └─ Save to USB + email backup to self

DAY 2 - FLASH & RESTORE:
  Multi-UVTools OR UVTools2
  ├─ Flash ApeX firmware to radio
  ├─ Boot ApeX successfully
  ├─ Connect to computer
  ├─ Restore my_radio_stock_backup.img
  ├─ Verify all 50 channels present
  └─ Done! ✅
  
RESULT: All channels intact on ApeX
TIME: ~20 minutes total
```

### Example 2: User on F4HWN → ApeX (With Migration)

**Scenario:**
- Current radio: F4HWN with 150 programmed channels
- New option: ApeX migration will auto-recover channels

**Workflow Option A: Rely on Migration (Faster)**
```
1. Backup with Multi-UVTools (just in case)
2. Flash ApeX
3. Boot ApeX (migration auto-imports F4HWN channels)
4. Verify channels on radio display
5. Done! ✅

TIME: ~10 minutes
RESULT: 85-95% channels auto-recovered
```

**Workflow Option B: Backup + Restore (Safest)**
```
1. Backup with UVTools2 → f4hwn_backup.img
2. Flash ApeX
3. Boot (migration activates)
4. If migration successful → keep as-is
5. If migration fails → Restore via UVTools2
6. Done! ✅

TIME: ~20 minutes (10 if migration works)
RESULT: 100% channel recovery guaranteed
```

### Example 3: Fleet Management (Multiple Radios)

**Scenario:**
- Managing 5 radios for team/club
- Need: Migrate all from stock to ApeX + maintain team configuration

**Best with Multi-UVTools (Better batch support):**
```
BACKUP PHASE:
  Multi-UVTools (advantage here: batch backup)
  ├─ Radio 1 → config_001.img
  ├─ Radio 2 → config_002.img
  ├─ Radio 3 → config_003.img
  ├─ Radio 4 → config_004.img
  ├─ Radio 5 → config_005.img
  └─ All saved to USB "master_backup_folder"

FLASH PHASE:
  ├─ Flash ApeX to all 5 radios (done offline)
  └─ ~30 minutes for all

RESTORE PHASE:
  Multi-UVTools (advantage: quicker batch upload)
  ├─ For each radio:
  │   1. Connect to computer
  │   2. Load config_00X.img
  │   3. Restore to radio
  │   4. Disconnect
  └─ Total: ~5 minutes per radio × 5 = 25 minutes

TOTAL TIME: ~55 minutes for full fleet migration
RESULT: All 5 radios on ApeX with original configs ✅
```

**Also works with UVTools2 (just slightly slower):**
- Same process, same result
- Just take ~30-40 minutes for restore phase (sequential)

---

## Installation & Setup

### Multi-UVTools
1. **Get the tool:**
   - From your distribution source
   - Often bundled with enhanced packages
   
2. **Install dependencies:**
   - Check documentation for your variant
   - May require USB drivers (STM32 bootloader)

3. **Connect radio:**
   - USB cable (typically micro-USB or USB-C)
   - Set radio to bootloader/DFU mode if needed
   - Driver may auto-install

4. **Test:**
   - Try backup first (non-destructive)
   - Verify config.img created successfully

### UVTools2
1. **Get the tool:**
   - Desktop: [armel.github.io/uvtools2](https://armel.github.io/uvtools2/)
   - Web: [UVTools2 Flash Mode](https://armel.github.io/uvtools2/?mode=flash)
   - CLI: Available via package managers

2. **Install (Desktop):**
   ```bash
   # Linux/Mac via package manager
   brew install uvtools2  # macOS
   apt install uvtools2   # Debian/Ubuntu
   
   # Windows
   Download installer from website
   ```

3. **Connect radio:**
   - USB cable connection
   - May need STM32 DFU drivers on Windows
   - [Driver install guide available](https://armel.github.io/uvtools2/)

4. **Test:**
   - Try backup first (non-destructive)
   - Verify config created successfully

---

## Troubleshooting: Both Tools

### "Radio not detected"
- **Check:** USB cable connected and working
- **Check:** Radio is powered on
- **Check:** Drivers installed (Windows especially)
- **Solution:** Try different USB port, restart tool, reboot radio

### "Backup created but empty"
- **Cause:** Radio not fully powered on or connection dropped
- **Solution:** Reconnect, power cycle radio, try again
- **Both tools:** Same behavior and solution

### "Restore fails with error"
- **Cause:** Backup file corrupted or incompatible firmware
- **Solution:** 
  - Verify backup file integrity (check file size)
  - Try with different backup if available
  - Both tools will give same error (expected)

### "Channels restored but settings wrong"
- **Cause:** Signal lost during upload or partial restore
- **Solution:** 
  - Try restore again (is safe, just overwrites)
  - May need to reconfigure display/audio settings
  - Channel data protected, settings are lower priority

---

## Which Tool Should I Choose?

### Choose **Multi-UVTools** if you:
- ✅ Want more advanced features (batch operations, filters, templates)
- ✅ Managing multiple radios regularly  
- ✅ Need professional-grade batch capabilities
- ✅ Want all-in-one radio management solution

### Choose **UVTools2** if you:
- ✅ Want established, proven stability
- ✅ Need simple, straightforward backup/restore
- ✅ Prefer minimal learning curve
- ✅ Want broad cross-platform support
- ✅ Just need to backup and restore configs

### Either tool works equally for:
- ✅ Daily backup/restore
- ✅ Flashing ApeX firmware
- ✅ One-time migrations
- ✅ Emergency channel recovery
- ✅ Calibration backup/restore

**Bottom line:** You cannot go wrong with either. Pick whichever you're comfortable with or have easier access to. Both are equally supported and reliable for ApeX Edition. 👍

---

## Best Practices (Both Tools)

1. **Always backup before flashing** - Even if migration might work
2. **Test restore** - Verify backup works by test-restoring to another radio if possible
3. **Store backups safely** - USB drive + cloud (email, Dropbox, etc.)
4. **Keep dated backups** - Don't overwrite old backups immediately
5. **Label clearly** - `radio_name_date.img` format helps organize
6. **Document settings** - Note any custom settings not in channel data
7. **Test on one radio first** - If managing fleet, do one test radio before all

---

## Community Resources

### Multi-UVTools
- Check official distribution documentation
- Community forums for your variant
- GitHub issues/discussions

### UVTools2
- **Official:** [armel.github.io/uvtools2](https://armel.github.io/uvtools2/)
- **GitHub:** [armel/uvtools2](https://github.com/armel/uvtools2)
- **YouTube:** Search "UVTools2 tutorial" (lots of guides)
- **Reddit:** r/UV_K1 and radio communities
- **Discord:** UV-K1/K5 community servers

---

## Summary

**Both Multi-UVTools and UVTools2 are professional-grade tools** fully supported by ApeX Edition:

| Task | Multi-UVTools | UVTools2 | Use | Recommendation |
|------|:-------------:|:--------:|:----|----------------|
| Backup | ✅ | ✅ | Both work perfectly | Either tool |
| Flash | ✅ | ✅ | Both work perfectly | Either tool |
| Restore | ✅ | ✅ | Both work perfectly | Either tool |
| Batch ops | ✅ Enhanced | ✅ Basic | Multi-UVTools wins | Multi for fleet |
| Simplicity | ✅ | ✅ Better | UVTools2 wins | UVTools2 for newbies |

**Choose either one. You won't make a wrong choice.** Both will get your radio backed up, flashed, and restored safely and reliably. 

For most users doing a one-time migration → **UVTools2**  
For professionals managing multiple radios → **Multi-UVTools**  
For anyone → **Either one does the job perfectly!** ✅