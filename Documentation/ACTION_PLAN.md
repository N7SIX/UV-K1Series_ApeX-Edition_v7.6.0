<!--
=====================================================================================
UV-K1 ApeX Edition - Prioritized Action Plan
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# UV-K1 ApeX Edition - Prioritized Action Plan

**Date:** March 29, 2026  
**Total Issues:** 185+ identified in comprehensive audit  
**Estimated Implementation Effort:** 4-5 weeks  
**Priority Levels:** CRITICAL (P0), HIGH (P1), MEDIUM (P2), LOW (P3)

---

## Quick Reference: Issue Severity Distribution

```
CRITICAL (P0): 5 issues  - Must fix before release
HIGH (P1):     6 issues  - Major refactoring needed
MEDIUM (P2):   60+ issues - Code quality improvements  
LOW (P3):      110+ issues - Nice-to-have optimizations
```

---

## Phase 1: Critical Safety Fixes (Days 1-3, ~16 hours)

### P0.1: NULL Pointer Dereference Guards
**File:** [App/app/spectrum.c](App/app/spectrum.c#L676-L679)  
**Effort:** 2 hours  
**Impact:** Prevents segmentation faults

**Tasks:**
- [ ] Add defensive null-checks for `gRxVfo`, `gCurrentVfo`, `gTxVfo`
- [ ] Create shared guard macro: `SAFE_VFO_ACCESS(vfo, fallback)`
- [ ] Test with mock invalid VFO states
- [ ] Update documentation with safety contract

**Code Template:**
```c
// New macro in radio.h:
#define SAFE_VFO_ACCESS(vfo, default_val) \
    ((vfo) ? (vfo) : (default_val))

// Apply in spectrum.c:
static uint16_t Rssi2Y(uint16_t rssi) {
    VFO_Info_t *safe_vfo = SAFE_VFO_ACCESS(gRxVfo, gVfoInfo);
    if (!safe_vfo) return 0;
    // ... rest of function
}
```

---

### P0.2: USB Circular Buffer Bounds Checking
**File:** [App/usb/usbd_cdc_if.c](App/usb/usbd_cdc_if.c#L113-L131)  
**Effort:** 3 hours  
**Impact:** Prevents USB buffer overflow attacks

**Tasks:**
- [ ] Define CDC_RX_BUFFER_SIZE constant
- [ ] Add bounds validation before pointer increment
- [ ] Implement overflow handler (drop packet or error callback)
- [ ] Add unit test for maximum buffer scenario
- [ ] Document buffer overflow behavior

**Code Template:**
```c
// In usbd_cdc_if.h:
#define CDC_RX_BUFFER_MAX_SIZE 4096

// In usbd_cdc_acm_bulk_out():
if ((pointer + size) > rx_buf->size) {
    // Buffer would overflow
    DEBUG_PRINTF("CDC RX buffer overflow: %u + %u > %u\n", 
                 pointer, size, rx_buf->size);
    return;  // Drop packet gracefully
}
pointer += size;
```

---

### P0.3: NULL Pointer Write Operation Fix
**File:** [App/usb/usbd_cdc_if.c](App/usb/usbd_cdc_if.c#L142)  
**Effort:** 1 hour  
**Impact:** Prevents undefined behavior in USB ZLP code path

**Tasks:**
- [ ] Verify hardware actually supports NULL pointer for ZLP
- [ ] Add conditional logic or use proper ZLP mechanism
- [ ] Document ZLP implementation contract
- [ ] Test with actual CDC host (Linux, Windows)

**Code Template:**
```c
void usbd_cdc_acm_bulk_in(uint8_t ep, uint32_t nbytes) {
    if ((nbytes % CDC_MAX_MPS) == 0 && nbytes) {
        // Send Zero-Length Packet (only if needed)
        #ifdef CDC_SUPPORTS_NULL_ZLP
        usbd_ep_start_write(CDC_IN_EP, NULL, 0);
        #else
        // Alternative: Use small buffer with zero content
        uint8_t zlp_buffer[1] = {0};
        usbd_ep_start_write(CDC_IN_EP, zlp_buffer, 0);
        #endif
    } else {
        ep_tx_busy_flag = false;
    }
}
```

---

### P0.4: EEPROM Casting Validation
**File:** [App/settings.c](App/settings.c#L225-L254)  
**Effort:** 2 hours  
**Impact:** Prevents DTMF buffer overflow

**Tasks:**
- [ ] Create validated cast function `SafeDTMFCast()`
- [ ] Add source buffer validation (null-terminated, valid chars)
- [ ] Add length checks before any cast operation
- [ ] Add unit test for malformed DTMF data
- [ ] Document DTMF data format contract

**Code Template:**
```c
// In dtmf.h:
bool DTMF_SafeCast(const uint8_t *source, uint8_t *dest, 
                   size_t dest_size) {
    if (!source || !dest || dest_size == 0) {
        return false;
    }
    
    // Validate actual source length
    size_t src_len = strlen((char*)source);
    if (src_len >= dest_size) {
        DEBUG_PRINTF("DTMF cast: source %u >= dest %u\n", src_len, dest_size);
        return false;
    }
    
    // Validate all characters are valid DTMF
    for (size_t i = 0; i < src_len; i++) {
        if (!DTMF_ValidateChar(source[i])) {
            return false;
        }
    }
    
    strncpy((char*)dest, (char*)source, dest_size);
    dest[dest_size-1] = '\0';
    return true;
}

// In settings.c:
if (!DTMF_SafeCast(source_data, gEeprom.dtmf.code[0], 
                   sizeof(gEeprom.dtmf.code[0]))) {
    // Log error and use safe default
    strncpy((char*)gEeprom.dtmf.code[0], "0", 
            sizeof(gEeprom.dtmf.code[0]));
}
```

---

### P0.5: Volatile Flag Race Condition Audit
**File:** [App/app/app.c](App/app/app.c#L1002) (and multiple other locations)  
**Effort:** 3 hours  
**Impact:** Ensures ISR flag updates visible to main loop

**Tasks:**
- [ ] Audit all flags modified by ISRs
- [ ] Mark all ISR-accessed flags as `volatile`
- [ ] Create checklist of all scheduler flags
- [ ] Run test suite: `tests/test_scheduler.cpp` to verify
- [ ] Document flag update contract

**Code Audit Script:**
```bash
#!/bin/bash
# Find all ISR declarations and their modified flags
grep -r "void.*ISR\|void.*Interrupt\|void.*Handler" App/ | grep -v ".h:" > /tmp/isrs.txt

# For each ISR, find variables it modifies
while IFS= read -r line; do
    func=$(echo "$line" | awk -F: '{print $2}' | sed 's/).*//;s/.*[ \t]//')
    echo "Checking ISR: $func"
    grep -n "$func" App/ -A 20 -B 2 | grep -E "=" | head -5
done < /tmp/isrs.txt
```

**Code Template:**
```c
// WRONG (compiler may optimize away polling)
static bool gScheduleFlag = false;

void APP_ISR_Handler() {
    gScheduleFlag = true;  // Write optimized away!
}

// CORRECT (volatile prevents optimization)
static volatile uint8_t gScheduleFlag = 0;

void APP_ISR_Handler() {
    gScheduleFlag = 1;  // Write guaranteed
}

// In main loop:
if (gScheduleFlag) {
    gScheduleFlag = 0;
    ProcessScheduledTask();
}
```

---

## Phase 2: High-Priority Architecture Refactoring (Days 4-9, ~24 hours)

### P1.1: Global Variable Coupling Refactoring
**File:** [App/app/app.c](App/app/app.c#L2257-L2270)  
**Effort:** 6 hours  
**Impact:** Improves testability, reduces side effects

**Tasks:**
- [ ] Create event subscription system
- [ ] Convert `gRequestSaveChannel` to callback pattern
- [ ] Audit all other global flags for similar patterns
- [ ] Add unit tests for callback mechanism
- [ ] Document event contract

**Implementation Steps:**

1. **Define Event System (2 hours):**
```c
// In app/events.h:
typedef enum {
    APP_EVENT_SAVE_CHANNEL,
    APP_EVENT_LOAD_CHANNEL,
    APP_EVENT_FREQUENCY_CHANGE,
    APP_EVENT_MODE_CHANGE,
} AppEventType_t;

typedef void (*AppEventCallback_t)(AppEventType_t event, void *data);

#define MAX_EVENT_SUBSCRIBERS 4

typedef struct {
    AppEventCallback_t callbacks[MAX_EVENT_SUBSCRIBERS];
    uint8_t count;
} AppEventBus_t;

extern AppEventBus_t gAppEventBus[APP_EVENT_MAX];

void APP_SubscribeEvent(AppEventType_t event, AppEventCallback_t callback);
void APP_UnsubscribeEvent(AppEventType_t event, AppEventCallback_t callback);
void APP_RaiseEvent(AppEventType_t event, void *data);
```

2. **Update Menu Code (1 hour):**
```c
// OLD (in menu.c):
void Menu_SaveChannel() {
    gRequestSaveChannel = true;  // Global flag
}

// NEW (in menu.c):
void Menu_SaveChannel() {
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &current_channel_index);
}
```

3. **Update App Handlers (2 hours):**
```c
// In app.c, during initialization:
void APP_Init() {
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, HandleSaveChannel);
    APP_SubscribeEvent(APP_EVENT_LOAD_CHANNEL, HandleLoadChannel);
    // ... other subscriptions
}

static void HandleSaveChannel(AppEventType_t event, void *data) {
    uint8_t channel_idx = *(uint8_t*)data;
    EEPROM_WriteChannel(channel_idx);
}
```

4. **Add Tests (1 hour):**
```c
// In tests/test_events.cpp:
void test_event_subscription() {
    bool callback_called = false;
    
    auto test_callback = [&](AppEventType_t e, void *d) {
        callback_called = true;
    };
    
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, test_callback);
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, NULL);
    
    ASSERT(callback_called == true);
}
```

---

### P1.2: Documented Hack Removal (REGA Feature)
**File:** [App/app/app.c](App/app/app.c#L143)  
**Effort:** 4 hours  
**Impact:** Eliminates ad-hoc conditional logic

**Tasks:**
- [ ] Create `features_config.h` with feature flags
- [ ] Move REGA logic to conditional feature section
- [ ] Add runtime feature detection
- [ ] Document feature configuration
- [ ] Test with REGA enabled/disabled

**Implementation:**
```c
// In features_config.h:
#ifndef ENABLE_FEATURE_REGA_UI
#define ENABLE_FEATURE_REGA_UI 0
#endif

// In app.c:
void APP_DrawSpecialElements() {
    #if ENABLE_FEATURE_REGA_UI
        DrawREGASpecialDisplayElement();
    #endif
}

// Compile-time feature control:
// cmake -DENABLE_FEATURE_REGA_UI=ON build/
```

---

### P1.3: Spectrum Transmit Feature Completion
**File:** [App/app/spectrum.c](App/app/spectrum.c#L2184)  
**Effort:** 8 hours  
**Impact:** Completes unimplemented feature

**Options:**

**Option A: Complete Implementation (Recommended, 8 hours)**
```c
// In spectrum.c:
static void StartSpectrumTransmit() {
    if (!gCurrentVfo) return;
    
    // Set VFO to current scan frequency
    gCurrentVfo->freq = CurrentScanFrequency;
    
    // Apply current bandwidth/modulation
    BK4819_SetFrequency(gCurrentVfo->freq);
    BK4819_SetModulationType(gCurrentVfo->modulation);
    BK4819_SetFilterBW(gCurrentVfo->bandwidth);
    
    // Start transmission
    BK4819_TxOn();
    
    // Update UI
    gUpdateDisplay = true;
    
    // Log action
    DEBUG_PRINTF("Transmit started: %u Hz\n", gCurrentVfo->freq);
}

// Add safety checks, timeout handling, etc.
```

**Option B: Defer/Remove (Faster, 2 hours)**
```c
// In spectrum.c, at line 2184:
// REMOVED: Transmit feature incomplete - use standard VFO transmit
// Users should enable transmit via main VFO UI instead
```

---

### P1.4: Loose Spectrum State Consolidation
**File:** [App/app/spectrum.c](App/app/spectrum.c#L156-L230)  
**Effort:** 5 hours  
**Impact:** Reduces global state variables from 30+ to 1 struct

**Tasks:**
- [ ] Create SpectrumState_t struct with all settings
- [ ] Add initialization function with defaults
- [ ] Replace 30+ global variables with struct members
- [ ] Update persistence code to save struct
- [ ] Add state validation function

**Code Refactoring:**

**Before (30+ globals):**
```c
static struct {
    uint16_t scanStepIndex;
    uint16_t stepsCount;
    uint16_t listenBw;
    // ... 27 more fields
} settings = {0};
```

**After (1 struct with clear dependency order):**
```c
// In spectrum.h:
typedef struct {
    // Core scanning parameters (initialize first)
    uint16_t scanStepIndex;
    uint16_t stepsCount;
    
    // Audio/RF parameters (depend on step size)
    uint16_t listenBw;
    uint8_t modulationType;
    
    // Signal measurement (depends on modulation)
    int16_t dbMin;
    int16_t dbMax;
    uint16_t rssiTriggerLevel;
    
    // Timing parameters (hardware dependent)
    uint16_t scanDelay;
    uint32_t frequencyChangeStep;
    
    // UI state
    uint8_t backlightState;
} SpectrumSettings_t;

// Add initialization with default values
void SpectrumSettings_Init(SpectrumSettings_t *state) {
    *state = (SpectrumSettings_t){
        .scanStepIndex = S_STEP_25_0kHz,
        .stepsCount = STEPS_64,
        .listenBw = BK4819_FILTER_BW_WIDE,
        .modulationType = MODULATION_FM,
        .dbMin = DISPLAY_DBM_MIN,
        .dbMax = DISPLAY_DBM_MAX,
        .rssiTriggerLevel = 150,
        .scanDelay = 3200,
        .frequencyChangeStep = 600000,
        .backlightState = 1,
    };
}

// Validate state after loading from EEPROM
bool SpectrumSettings_Validate(const SpectrumSettings_t *state) {
    if (!state) return false;
    if (state->scanStepIndex > S_STEP_100_0kHz) return false;
    if (state->dbMin >= state->dbMax) return false;
    // ... other validation
    return true;
}
```

---

### P1.5: Weak Signal Detection Optimization
**File:** [App/app/spectrum.c](App/app/spectrum.c#L928-L953)  
**Effort:** 4 hours  
**Impact:** Improves scan reliability for weak signals

**Implementation:**
```c
// In spectrum.h:
typedef enum {
    SENSITIVITY_VERY_LOW = 0,   // Only catch strong signals (>-80dBm)
    SENSITIVITY_LOW,
    SENSITIVITY_MEDIUM,         // Default (-100dBm)
    SENSITIVITY_HIGH,
    SENSITIVITY_VERY_HIGH,      // Detect minimal signals (-120dBm)
} SignalSensitivity_t;

// Sensitivity-to-RSSI mapping
static const uint16_t RSSI_TRIGGER_TABLE[] = {
    [SENSITIVITY_VERY_LOW]   = dbm2rssi(-80),   // 160 RSSI units
    [SENSITIVITY_LOW]        = dbm2rssi(-100),  // 120 RSSI units
    [SENSITIVITY_MEDIUM]     = dbm2rssi(-110),  // 100 RSSI units (default)
    [SENSITIVITY_HIGH]       = dbm2rssi(-120),  // 80 RSSI units
    [SENSITIVITY_VERY_HIGH]  = dbm2rssi(-130),  // 60 RSSI units
};

// Adaptive auto-trigger with hysteresis
void AutoTriggerLevel(SignalSensitivity_t sensitivity) {
    uint16_t trigger = RSSI_TRIGGER_TABLE[sensitivity];
    
    // Add hysteresis to prevent flickering
    if (CurrentRSSI > trigger + HYSTERESIS_MARGIN) {
        // Signal detected
        gSpectrum.triggerActive = true;
        gSpectrum.lastTriggerTime = systick_ms;
    } else if (CurrentRSSI < trigger - HYSTERESIS_MARGIN) {
        // Signal lost
        gSpectrum.triggerActive = false;
    }
    // else: hysteresis zone, maintain current state
}
```

---

## Phase 3: Performance Optimization (Days 10-14, ~20 hours)

### P2.1: EEPROM Write Non-Blocking Scheduler 
**File:** [App/settings.c](App/settings.c#L1185-L1200)  
**Effort:** 8 hours  
**Impact:** Eliminates 30ms UI freeze

**Priority:** This is the single biggest UI responsiveness issue.

**Implementation:**

1. **Create EEPROM Write Queue (2 hours):**
```c
// In settings.h:
#define EEPROM_WRITE_QUEUE_SIZE 4
#define EEPROM_BATCH_SIZE 64  // Small batches (~1ms each)

typedef struct {
    uint8_t data[EEPROM_BATCH_SIZE];
    uint16_t address;
    uint16_t size;
} EEPROMWriteRequest_t;

typedef struct {
    EEPROMWriteRequest_t queue[EEPROM_WRITE_QUEUE_SIZE];
    uint8_t write_idx;  // Next position to write
    uint8_t read_idx;   // Current position processing
    uint8_t count;      // Items in queue
} EEPROMWriteQueue_t;

extern EEPROMWriteQueue_t gEEPROMWriteQueue;

bool EEPROM_QueueWrite(uint16_t address, const uint8_t *data, uint16_t size);
```

2. **Update Settings Module (2 hours):**
```c
// In settings.c - OLD blocking code:
void EEPROM_WriteAllChannels() {
    for (int i = 0; i < CHANNEL_COUNT; i += 20) {
        EEPROM_WriteBuffer(BASE_ADDR + i*16, channelData + i*16, 320);
        // UI BLOCKS HERE FOR ~3ms × 10 = ~30ms total
    }
}

// NEW non-blocking code:
void EEPROM_QueueAllChannels() {
    for (int i = 0; i < CHANNEL_COUNT; i += EEPROM_BATCH_SIZE) {
        uint16_t batch_size = (i + EEPROM_BATCH_SIZE <= CHANNEL_COUNT) 
                            ? EEPROM_BATCH_SIZE 
                            : (CHANNEL_COUNT - i);
        EEPROM_QueueWrite(BASE_ADDR + i*16, channelData + i*16, batch_size*16);
        // Returns immediately, no blocking
    }
}
```

3. **Add Scheduler Integration (2 hours):**
```c
// In app.c, in APP_TimeSlice500ms():
void APP_ProcessEEPROMQueue() {
    if (gReducedService != 0) {
        return;  // Skip if radio busy
    }
    
    if (gEEPROMWriteQueue.count == 0) {
        return;  // Queue empty
    }
    
    // Process one batch (~1-2ms, non-blocking UI)
    EEPROMWriteRequest_t *req = &gEEPROMWriteQueue.queue[gEEPROMWriteQueue.read_idx];
    EEPROM_WriteBuffer(req->address, req->data, req->size);
    
    // Advance queue
    gEEPROMWriteQueue.read_idx++;
    gEEPROMWriteQueue.count--;
    
    // Debug logging
    DEBUG_PRINTF("EEPROM batch written: %u bytes @ 0x%04X\n", req->size, req->address);
}
```

4. **Add Tests (2 hours):**
```c
// In tests/test_eeprom_queue.cpp:
void test_queue_non_blocking() {
    auto start_time = systick_ms;
    
    // Queue 10 batches (320 bytes total)
    EEPROM_QueueAllChannels();
    
    auto queue_time = systick_ms - start_time;
    ASSERT(queue_time < 5);  // Should be <5ms, not 30ms
    
    // Verify queue length
    ASSERT(gEEPROMWriteQueue.count == 10);
}
```

**Fallback Options if Hardware Doesn't Support:**
- Option B: Increase batch size from 320B to 512B (reduce operations 10→7, saves ~9ms)
- Option C: Defer all EEPROM writes until radio powers off (problematic for recovery)

---

### P2.2: Adaptive PLL Settling Delay
**File:** [App/app/spectrum.c](App/app/spectrum.c#L1087)  
**Effort:** 2 hours  
**Impact:** ~10-15% faster scanning

**Implementation:**
```c
// In spectrum.h:
typedef struct {
    uint32_t last_frequency;
    uint32_t settling_time_us;
} FrequencyChangeContext_t;

static FrequencyChangeContext_t gFreqChangeCtx = {0};

// Adaptive settling calculation
uint32_t CalculateSettlingTime(uint32_t freq_delta_hz) {
    // PLL settling depends on frequency change magnitude
    // Smaller changes need less settling time
    
    if (freq_delta_hz < 10000) {
        return 50;      // 50us for <10kHz change
    } else if (freq_delta_hz < 100000) {
        return 100;     // 100us for <100kHz change
    } else if (freq_delta_hz < 1000000) {
        return 200;     // 200us for <1MHz change
    } else {
        return 400;     // 400us for larger changes
    }
}

// In spectrum measurement loop:
void MeasureFrequency(uint32_t new_freq) {
    if (gFreqChangeCtx.last_frequency == 0) {
        gFreqChangeCtx.last_frequency = new_freq;
        gFreqChangeCtx.settling_time_us = 400;
    } else {
        uint32_t delta = abs((int32_t)(new_freq - gFreqChangeCtx.last_frequency));
        gFreqChangeCtx.settling_time_us = CalculateSettlingTime(delta);
        gFreqChangeCtx.last_frequency = new_freq;
    }
    
    // Adaptive delay
    SYSTEM_DelayUs(gFreqChangeCtx.settling_time_us);
    
    // Continue measurement...
}
```

---

### P2.3: Efficient Microsecond Delay Implementation
**File:** [App/app/spectrum.c](App/app/spectrum.c#L69-L90)  
**Effort:** 3 hours  
**Impact:** More accurate delays, lower CPU overhead

**Current Issue:** Uses 12× NOP instruction overhead

**Solution:**
```c
// Use SysTick timer instead of loop
// In spectrum.c:

#include "py32f071_hal.h"

void SYSTEM_DelayUs(uint32_t us) {
    uint32_t ticks_per_us = SystemCoreClock / 1000000;  // e.g., 48 ticks/us @ 48MHz
    uint32_t target_ticks = us * ticks_per_us;
    
    uint32_t start_val = SysTick->VAL;
    uint32_t current;
    
    // SysTick counts DOWN
    while (1) {
        current = SysTick->VAL;
        uint32_t elapsed = (current < start_val) 
                         ? (start_val - current)
                         : (start_val + SysTick->LOAD - current);
        
        if (elapsed >= target_ticks) {
            break;
        }
    }
}
```

**Or use more efficient HAL function:**
```c
// If available in PY32F071 HAL:
void SYSTEM_DelayUs(uint32_t us) {
    HAL_Delay(us / 1000);  // Round up milliseconds
    // Note: Less accurate for <1ms delays
}

// For <1ms precision, use:
#define SYSTEM_DELAY_US_FAST(us) \
    do { \
        for (uint32_t i = 0; i < (us * 6); i++) {  /* 6× multiplier at 48MHz */ \
            __asm__ volatile("nop"); \
        } \
    } while(0)
```

**Verification:**
```c
// Test delay accuracy
#ifdef DEBUG
void TestDelayAccuracy() {
    uint32_t start = SysTick->VAL;
    SYSTEM_DelayUs(1000);  // Request 1ms
    uint32_t end = SysTick->VAL;
    uint32_t actual_us = /* convert to microseconds */;
    
    if (actual_us > 1050 || actual_us < 950) {
        DEBUG_PRINTF("Warning: Delay accuracy %.1f%%\n", (actual_us / 1000.0) * 100);
    }
}
#endif
```

---

## Phase 4: Code Quality (Days 15+, Ongoing)

### P2.4: sprintf → snprintf Automated Migration
**Effort:** 2 hours (automated script + verification)  
**Files Affected:** spectrum.c (40+ instances), others

**Automated Migration Script:**
```bash
#!/bin/bash
# Safely replace sprintf with snprintf

for file in $(find App -name "*.c" -o -name "*.h"); do
    echo "Processing: $file"
    
    # Find all sprintf calls with potential buffer size
    grep -n "sprintf(" "$file" | while read line; do
        line_num=$(echo "$line" | cut -d: -f1)
        echo "  Line $line_num: $line"
    done
done

# Generate report for manual review
echo ""
echo "=== Manual Review Required ==="
echo "Not all sprintf→snprintf conversions are safe."
echo "Please review each instance in the generated report."
```

**Manual Review Checklist for Each instance:**
```
For each sprintf(buffer, format, ...):
- [ ] Buffer pointer is valid
- [ ] Buffer size is known
- [ ] Replace with snprintf(buffer, sizeof(buffer), format, ...)
- [ ] Test with format strings that produce output == sizeof(buffer)
- [ ] Verify no off-by-one errors (snprintf returns count without null)
```

---

### P2.5: Profanity Removal from dtmf.c
**File:** [App/app/dtmf.c](App/app/dtmf.c#L314), [App/app/dtmf.c](App/app/dtmf.c#L320), [App/app/dtmf.c](App/app/dtmf.c#L348)  
**Effort:** 30 minutes

**Search and Replace:**
```bash
# Find all profanity
grep -n "bugger\|shit\|oooerr" App/app/dtmf.c

# Replace with professional comments
sed -i 's/.*bugger.*/    \/\/ DTMF decode retry on signal timeout/g' App/app/dtmf.c
sed -i 's/.*shit.*/    \/\/ DTMF parser state synchronization/g' App/app/dtmf.c
sed -i 's/.*oooerr.*/    \/\/ DTMF sequence validation/g' App/app/dtmf.c

# Verify changes
grep -n "DTMF\|TODO" App/app/dtmf.c
```

---

### P3.1: Deprecated Function Cleanup
**File:** [App/app/spectrum.c](App/app/spectrum.c#L1504)  
**Effort:** 2 hours

**Tasks:**
- [ ] Verify no callers of DrawSpectrum()
- [ ] Search codebase: `grep -r "DrawSpectrum" App/`
- [ ] Remove deprecated function
- [ ] Estimate binary size savings: ~200-300 bytes
- [ ] Verify functionality unchanged

```bash
# Check for any references to deprecated function
grep -r "DrawSpectrum" App/
# Should return no results except the definition itself

# After removal
grep -c "DrawSpectrum" App/app/spectrum.c
# Should return 0
```

---

### P3.2: Test Suite Integration
**Files:** test_scheduler.cpp, test_st7565.cpp, test_update_flags.cpp  
**Effort:** 3 hours

**CMakeLists.txt Integration:**
```cmake
# In /tests/CMakeLists.txt:

enable_testing()

# Find Google Test (or use local version)
find_package(GTest REQUIRED)

# Create test executable
add_executable(codetest
    test_runner.cpp
    test_scheduler.cpp
    test_st7565.cpp
    test_update_flags.cpp
)

target_include_directories(codetest PRIVATE
    ${CMAKE_SOURCE_DIR}/App
    ${CMAKE_SOURCE_DIR}/Core/Inc
)

target_link_libraries(codetest
    GTest::Main
    GTest::GTest
)

# Register with CTest
add_test(NAME unit_tests COMMAND codetest)

# Optional: Add to main build
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS codetest
)
```

**Run tests:**
```bash
cd build/ApeX
cmake ..
make
ctest --output-on-failure
```

---

### P3.3: Assertion Coverage Enhancement
**File:** [Core/Inc/py32_assert.h](Core/Inc/py32_assert.h)  
**Effort:** 2 hours

**Improved Strategy:**
```c
// In py32_assert.h:

// CRITICAL assertions (always checked)
#define ASSERT_CRITICAL(expr, msg) \
    do { \
        if (!(expr)) { \
            AssertFailed_Critical((uint8_t*)__FILE__, __LINE__, msg); \
        } \
    } while(0)

// DEBUG assertions (only in Debug builds)
#ifdef DEBUG
#define ASSERT_DEBUG(expr, msg) ASSERT_CRITICAL(expr, msg)
#else
#define ASSERT_DEBUG(expr, msg) ((void)0)
#endif

// Helper functions
void AssertFailed_Critical(uint8_t *file, uint32_t line, const char *msg);

#ifdef DEBUG
void AssertFailed_Debug(uint8_t *file, uint32_t line, const char *msg);
#endif

// Usage in code:
void ProcessDTMF(const char *code, size_t size) {
    ASSERT_CRITICAL(code != NULL, "DTMF code cannot be null");
    ASSERT_CRITICAL(size > 0 && size <= DTMF_MAX_LEN, "DTMF code size invalid");
    ASSERT_DEBUG(strlen(code) == size, "DTMF code length mismatch");
    
    // Process...
}
```

---

## Summary: Week-by-Week Implementation Plan

**Week 1 (Days 1-5): Critical Fixes**
- Mon-Tue: NULL pointer guards, USB buffer bounds
- Wed: NULL pointer write, EEPROM casting  
- Thu: Volatile flag audit
- Fri: Testing & verification

**Week 2-3 (Days 6-15): Architecture Refactoring**
- Refactor global coupling → events
- Remove REGA hack
- Complete/decide on transmit feature
- Consolidate spectrum state

**Week 4 (Days 16-20): Performance**
- Non-blocking EEPROM queue
- Adaptive delays
- Efficient microsecond function
- Test integration

**Week 5+ (Ongoing): Code Quality**
- sprintf→snprintf migration
- Profanity removal
- Deprecated functions
- Assertion coverage

---

## Validation & Sign-Off

**Before Each Phase:**
- [ ] Create feature branch
- [ ] Make targeted changes
- [ ] Run full test suite: `ctest --output-on-failure`
- [ ] Build all presets: `./compile-with-docker.sh All`
- [ ] Verify 0 errors, 0 warnings
- [ ] Create pull request with detailed description
- [ ] Get code review + test validation

**Before Release:**
- [ ] All P0/P1 issues resolved
- [ ] Size/performance benchmarks documented
- [ ] Integration tests passing
- [ ] User manual updated
- [ ] Release notes generated
- [ ] Version bumped (semantic versioning)

---

**End of Action Plan**

*Last Updated: March 29, 2026*
*Next Review: After Phase 1 completion*
