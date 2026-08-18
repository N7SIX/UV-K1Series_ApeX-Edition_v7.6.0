# Variant Consolidation Plan
## Remove Stock/NOGIT Variant, Keep Only ApeX

> **Date:** 2026-08-06  
> **Status:** ✅ **IMPLEMENTED**  
> **Goal:** Simplify build system by removing stock firmware variant support  
> **Current state:** Single variant — **ApeX only** (Stock/NOGIT removed)

---

## Implementation Status (v7.6.9D)

All changes described in this plan have been **fully implemented**:

- ✅ `CMakeLists.txt` — Stock/NOGIT variant conditionals removed; ApeX strings always set
- ✅ `App/CMakeLists.txt` — Stock variant package name conditional removed
- ✅ `CMakePresets.json` — Stock presets removed; only ApeX presets remain
- ✅ `README.md` — Updated to reflect single ApeX-only build
- ✅ All working ApeX features restored (charging, CTCSS, charge level, NOAA, alarm, DTMF)
- ✅ Conflicting features (`ENABLE_REGA`, `ENABLE_EXTRA_UART_CMD`) disabled

**Result:** The repository now builds exclusively as the ApeX Edition. The two-variant system has been fully consolidated.

---

## Current Variant System

The firmware now supports **one build variant** (after consolidation):

### 1. ApeX Variant (only variant)
- **Author:** EGZUMER+N7SIX
- **Version:** v0.22+v7.6.0
- **Edition:** Custom
- **Package name:** `n7six.ApeX-k1.v7.6.0`
- **Features:** All N7SIX mods enabled (CW, spectrum, screenshot, audio bar, charging, CTCSS tail phase, charge level, NOAA, alarm, DTMF calling)
- **Size:** ~110-120 KB flash

### 2. Stock/NOGIT Variant (`ENABLE_FEAT_N7SIX=OFF`) — **REMOVED (v7.6.9D)**
- **Status:** ❌ Removed — no longer supported in this repository
- **Alternative:** Use upstream Quansheng repository for stock firmware

---

## Why Was Stock Variant Removed?

1. **Maintenance burden:** Two code paths to test and maintain
2. **User confusion:** Which variant to download/use?
3. **Repository purpose:** This is the ApeX edition repository
4. **Build complexity:** Conditional compilation adds complexity
5. **Archive bloat:** Multiple builds in `archive/` for both variants

---

## Implemented Changes

### 1. Remove Variant Conditionals from CMakeLists.txt

**Current code (lines 55-84):**
```cmake
if(ENABLE_FEAT_N7SIX)
    if(NOT AUTHOR_STRING_1)
        set(AUTHOR_STRING_1 "EGZUMER")
    endif()
    if(NOT AUTHOR_STRING_2)
        set(AUTHOR_STRING_2 "N7SIX")
    endif()
    if(NOT VERSION_STRING_1)
        set(VERSION_STRING_1 "v0.22")
    endif()
    if(NOT VERSION_STRING_2)
        set(VERSION_STRING_2 "v4.2")
    endif()
    if(NOT EDITION_STRING)
        set(EDITION_STRING "Custom")
    endif()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "${AUTHOR_STRING_1}+${AUTHOR_STRING_2}")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING ${VERSION_STRING_2})
    endif()
else()
    if(NOT AUTHOR_STRING)
        set(AUTHOR_STRING "EGZUMER")
    endif()
    if(NOT VERSION_STRING)
        set(VERSION_STRING "NOGIT")
    endif()
endif()
```

**Replace with:**
```cmake
# ApeX Edition - always enabled
if(NOT AUTHOR_STRING_1)
    set(AUTHOR_STRING_1 "EGZUMER")
endif()
if(NOT AUTHOR_STRING_2)
    set(AUTHOR_STRING_2 "N7SIX")
endif()
if(NOT VERSION_STRING_1)
    set(VERSION_STRING_1 "v0.22")
endif()
if(NOT VERSION_STRING_2)
    set(VERSION_STRING_2 "v4.2")
endif()
if(NOT EDITION_STRING)
    set(EDITION_STRING "Custom")
endif()
if(NOT AUTHOR_STRING)
    set(AUTHOR_STRING "${AUTHOR_STRING_1}+${AUTHOR_STRING_2}")
endif()
if(NOT VERSION_STRING)
    set(VERSION_STRING ${VERSION_STRING_2})
endif()
```

---

### 2. Remove Firmware Package Name Conditional

**Current code (lines 92-98):**
```cmake
if(NOT FIRMWARE_PACKAGE_NAME)
    if(ENABLE_FEAT_N7SIX)
        set(FIRMWARE_PACKAGE_NAME "n7six.ApeX-k1.${VERSION_STRING_2}")
    else()
        set(FIRMWARE_PACKAGE_NAME "${EXE_NAME}.${VERSION_STRING}")
    endif()
endif()
```

**Replace with:**
```cmake
if(NOT FIRMWARE_PACKAGE_NAME)
    set(FIRMWARE_PACKAGE_NAME "n7six.ApeX-k1.${VERSION_STRING_2}")
endif()
```

---

### 3. Simplify App/CMakeLists.txt

**Current code (lines 68-77):**
```cmake
if(ENABLE_FEAT_N7SIX)
    target_compile_definitions(App INTERFACE 
        "AUTHOR_STRING_1=\"${AUTHOR_STRING_1}\""
        "AUTHOR_STRING_2=\"${AUTHOR_STRING_2}\""
        "VERSION_STRING_1=\"${VERSION_STRING_1}\""
        "VERSION_STRING_2=\"${VERSION_STRING_2}\""
        "EDITION_STRING=\"${EDITION_STRING}\""
        ALERT_TOT=10
    )
endif()
```

**Replace with:**
```cmake
target_compile_definitions(App INTERFACE 
    "AUTHOR_STRING_1=\"${AUTHOR_STRING_1}\""
    "AUTHOR_STRING_2=\"${AUTHOR_STRING_2}\""
    "VERSION_STRING_1=\"${VERSION_STRING_1}\""
    "VERSION_STRING_2=\"${VERSION_STRING_2}\""
    "EDITION_STRING=\"${EDITION_STRING}\""
    ALERT_TOT=10
)
```

---

### 4. Remove Stock Firmware Build Presets

**Files to update:**
- `CMakePresets.json` - Remove stock presets, keep only ApeX presets

**Current structure (example):**
```json
{
    "name": "ApeX",
    "configurePreset": "ApeX",
    "buildPreset": "ApeX",
    "binaryDir": "${sourceDir}/build/ApeX"
},
{
    "name": "Stock",
    "configurePreset": "Stock",
    "buildPreset": "Stock",
    "binaryDir": "${sourceDir}/build/Stock"
}
```

**Replace with:**
```json
{
    "name": "ApeX",
    "configurePreset": "ApeX",
    "buildPreset": "ApeX",
    "binaryDir": "${sourceDir}/build/ApeX"
}
```

---

### 5. Update CMakePresets.json Configure Presets

**Current structure:**
```json
{
    "name": "ApeX",
    "generator": "Ninja",
    "cacheVariables": {
        "ENABLE_FEAT_N7SIX": "ON",
        "CMAKE_BUILD_TYPE": "Release"
    }
},
{
    "name": "Stock",
    "generator": "Ninja",
    "cacheVariables": {
        "ENABLE_FEAT_N7SIX": "OFF",
        "CMAKE_BUILD_TYPE": "Release"
    }
}
```

**Replace with:**
```json
{
    "name": "ApeX",
    "generator": "Ninja",
    "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
    }
}
```

---

### 6. Clean Up Documentation

**Files to update:**
- `README.md` - Remove references to stock firmware builds
- `documentation/` - Update build instructions

**Remove sections like:**
```markdown
## Building Stock Firmware
cmake -B build/Stock -DENABLE_FEAT_N7SIX=OFF
```

**Replace with:**
```markdown
## Building
cmake -B build/ApeX
cmake --build build/ApeX
```

---

### 7. Archive Old Builds (Optional)

**Option A:** Keep archives for reference
- Move `archive/builds/` to `archive/historical_builds/`
- Document that these are historical references only

**Option B:** Delete old builds
- Remove `archive/builds/` directory entirely
- Saves ~50-100 MB disk space

**Recommendation:** Keep archives but move to `archive/historical/` for reference.

---

## Impact Analysis

### Benefits
1. **Simplified build system** - One less variant to maintain
2. **Clearer purpose** - Repository is explicitly ApeX-only
3. **Smaller documentation** - Less build instructions
4. **Faster CI/CD** - Only one build variant to test
5. **Reduced confusion** - Users know exactly what to download

### Risks
1. **Users wanting stock firmware** - Must build from upstream Quansheng repository
2. **Historical reference loss** - Mitigated by keeping archives

### Flash/RAM Impact
- **None** - This is a build system change only
- Firmware features and size remain identical

---

## Implementation Steps

### Step 1: Update CMakeLists.txt
1. Remove `else()` branch in root `CMakeLists.txt` (lines 77-84)
2. Remove conditional firmware package name (lines 92-98)
3. Remove conditional in `App/CMakeLists.txt` (lines 68-77)
4. Test build: `cmake -B build && cmake --build build`

### Step 2: Update CMakePresets.json
1. Remove "Stock" configure preset
2. Remove "Stock" build preset
3. Remove "Stock" test preset (if exists)
4. Update default preset if needed

### Step 3: Update Documentation
1. Update `README.md` build instructions
2. Update any other docs referencing stock builds
3. Add note about variant consolidation

### Step 4: Clean Up Archives
1. Move `archive/builds/` to `archive/historical_builds/`
2. Update `.gitignore` if needed

### Step 5: Test
1. Verify ApeX build works
2. Verify firmware package name is correct
3. Verify version strings are correct
4. Test on hardware if available

---

## Migration Guide for Users

**If you were building stock firmware:**

**Before:**
```bash
cmake -B build/Stock -DENABLE_FEAT_N7SIX=OFF
cmake --build build/Stock
```

**After:**
```bash
# Stock firmware is no longer supported in this repository
# Use the upstream Quansheng repository instead:
# https://github.com/notfair/uv-k5-firmware
```

**If you were building ApeX firmware:**

**Before:**
```bash
cmake -B build/ApeX -DENABLE_FEAT_N7SIX=ON
cmake --build build/ApeX
```

**After:**
```bash
cmake -B build/ApeX
cmake --build build/ApeX
# ENABLE_FEAT_N7SIX is now always ON
```

---

## Conclusion

Consolidating to a single ApeX variant simplifies the build system, reduces maintenance burden, and clarifies the repository's purpose. The change is low-risk and preserves all ApeX features and functionality.

**Estimated effort:** 2-3 hours  
**Risk:** Low  
**Benefit:** Simplified build system, clearer project scope