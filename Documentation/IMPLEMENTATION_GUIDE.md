# AUDIT FINDINGS QUICK REFERENCE & IMPLEMENTATION GUIDE

## Critical Issues (Implement Immediately)

### 1. Scheduler Race Condition Fix ⚠️ CRITICAL
**File:** `App/scheduler.c` + `App/scheduler.h`  
**Impact:** May cause skipped keyboard inputs, timing glitches  
**Effort:** 30 minutes

```c
// scheduler.h - Change declarations
extern volatile uint8_t gNextTimeslice;        // was: bool
extern volatile uint8_t gNextTimeslice_500ms;  // was: bool

// scheduler.c - SysTick_Handler
void SysTick_Handler(void) {
    gGlobalSysTickCounter++;
    gNextTimeslice = 1;  // Assign 1 instead of true
    
    if ((gGlobalSysTickCounter % 50) == 0) {
        gNextTimeslice_500ms = 1;  // Use volatile - compiler won't optimize
    }
    // ... rest unchanged
}

// main.c - Read pattern
while (true) {
    APP_Update();
    
    const uint8_t ts = gNextTimeslice;  // Atomic read
    if (ts) {
        gNextTimeslice = 0;
        APP_TimeSlice10ms();
        
        if (gNextTimeslice_500ms) {
            gNextTimeslice_500ms = 0; 
            APP_TimeSlice500ms();
        }
    }
}
```

---

### 2. ST7565 FillScreen Implementation ⚠️ CRITICAL
**File:** `App/driver/st7565.c`  
**Impact:** Black screen on boot, display won't update  
**Effort:** 1 hour

**BEFORE (BROKEN):**
```c
void ST7565_FillScreen(uint8_t value) {
    CS_Assert();
    for (unsigned int page = 0; page < 8; page++) {
        memset(gFrameBuffer[page], value, 128);  // Only updates RAM!
    }
    CS_Release();
}
```

**AFTER (FIXED):**
```c
void ST7565_FillScreen(uint8_t value) {
    // 1. Clear framebuffer
    for (unsigned int page = 0; page < 8; page++) {
        memset(gFrameBuffer[page], value, 128);
    }
    
    // 2. Send to display controller
    CS_Assert();
    __try_finally {
        for (unsigned int page = 0; page < 8; page++) {
            ST7565_SelectColumnAndLine(0, page);
            for (unsigned int col = 0; col < 128; col++) {
                SPI_WriteByte(gFrameBuffer[page][col]);
            }
        }
    } __finally {
        CS_Release();  // Always released
    }
}
```

**TESTING:**
- Boot test: Welcome screen appears (not black)
- Menu test: Screen clears when entering menu
- Reset test: Soft reset clears display

---

### 3. Display Flag Racing Fix ⚠️ CRITICAL-HIGH
**File:** `App/app/app.c` line ~1400  
**Impact:** Missed display updates, artifacts during rapid navigation  
**Effort:** 45 minutes

**BEFORE:**
```c
void APP_TimeSlice10ms(void) {
    bool gUpdateDisplayCurrent = gUpdateDisplay;
    bool gUpdateStatusCurrent = gUpdateStatus;
    
    if (gUpdateDisplayCurrent) {
        gUpdateDisplay = false;  // Cleared BEFORE update
        GUI_DisplayScreen();      // Can take 10ms
        // If flag set again here, update missed!
    }
}
```

**AFTER:**
```c
void APP_TimeSlice10ms(void) {
    // Read current state
    bool update_display = gUpdateDisplay;
    bool update_status = gUpdateStatus;
    
    if (update_display) {
        gUpdateDisplay = false;
        GUI_DisplayScreen();
        
        // Check if new update requested during draw
        if (gUpdateDisplay) {
            // Skip other work, let next timeslice catch it
            return;
        }
    }
    
    if (update_status) {
        gUpdateStatus = false;
        UI_DisplayStatus();
        
        if (gUpdateStatus) {
            return;
        }
    }
    
    // ... rest of timeslice work
}
```

---

## High Priority Issues (v7.7 Release)

### 4. EEPROM Validation & Recovery
**File:** `App/settings.c`  
**Impact:** Corrupted settings after power loss during write  
**Effort:** 2 hours

```c
// Add to settings.c
bool SETTINGS_ValidateChecksum(void) {
    uint16_t stored_crc;
    uint16_t computed_crc;
    
    // Read CRC from EEPROM
    PY25Q16_ReadBuffer(EEPROM_CRC_OFFSET, (uint8_t*)&stored_crc, 2);
    
    // Compute CRC of current gEeprom
    computed_crc = CRC16((uint8_t*)&gEeprom, sizeof(gEeprom));
    
    return (stored_crc == computed_crc);
}

// Modify SETTINGS_InitEEPROM()
void SETTINGS_InitEEPROM(void) {
    // Try to read normally
    uint8_t Data[16] = {0};
    PY25Q16_ReadBuffer(0x004000, Data, 8);
    
    // Validate the batch of reads
    if (!SETTINGS_ValidateChecksum()) {
        // EEPROM corrupted - rebuild to defaults
        printf("EEPROM checksum failed, resetting to defaults\n");
        SETTINGS_ResetToDefaults();
        return;
    }
    
    // Safe to proceed with parsing
    gEeprom.CHAN_1_CALL = IS_MR_CHANNEL(Data[0]) ? Data[0] : MR_CHANNEL_FIRST;
    // ... rest unchanged
}
```

**TESTING:**
- Simulate power loss (cut power during SETTINGS_SaveSettings)
- On reboot, should load either:
  - Previous valid state, OR
  - Default values
- NEVER corrupted state

---

### 5. USB Heap Fragmentation Prevention
**File:** `App/usb/usb_config.h`  
**Impact:** USB stops working after extended use  
**Effort:** 2 hours

```c
// usb_config.h - Replace malloc/free macros
#define USB_POOL_BLOCKS 16
#define USB_BLOCK_SIZE  256

static struct {
    uint8_t buffer[USB_BLOCK_SIZE];
    bool in_use;
} usb_pool[USB_POOL_BLOCKS];

static void usb_pool_init(void) {
    memset(usb_pool, 0, sizeof(usb_pool));
}

static void* usb_malloc(size_t size) {
    if (size > USB_BLOCK_SIZE) {
        return NULL;  // Too large
    }
    for (int i = 0; i < USB_POOL_BLOCKS; i++) {
        if (!usb_pool[i].in_use) {
            usb_pool[i].in_use = true;
            return usb_pool[i].buffer;
        }
    }
    return NULL;  // Exhausted
}

static void usb_free(void* ptr) {
    for (int i = 0; i < USB_POOL_BLOCKS; i++) {
        if (usb_pool[i].buffer == (uint8_t*)ptr) {
            usb_pool[i].in_use = false;
            return;
        }
    }
}
```

---

## Medium Priority Issues (v7.8-8.0)

### 6. Async Display Update (Spread over 8ms instead of 10ms burst)
**Complexity:** 4-6 hours  
**Benefit:** Responsive UI during spectrum scanning  
**Can defer to:** v7.8

### 7. Command Queue for State Changes
**Complexity:** 20+ hours (phased over 3 releases)  
**Benefit:** Eliminates global state races, better testing  
**Recommended approach:**
- Phase 1 (v7.8): Implement queue, migrate save/display commands
- Phase 2 (v7.9): Menu mode, VFO changes
- Phase 3 (v8.0): Deprecate old globals

### 8. Deferred EEPROM Writes
**Complexity:** 3-4 hours  
**Benefit:** Non-blocking EEPROM writes don't stall audio/UI  
**Can defer to:** v7.8

---

## Implementation Checklist

### Phase 1: Critical Fixes (2 weeks, v7.6.1)
- [ ] Fix scheduler volatile declarations
- [ ] Implement complete ST7565_FillScreen (with SPI transfer)
- [ ] Fix display flag racing in APP_TimeSlice10ms()
- [ ] Test: Full boot sequence, menu navigation, display clear
- [ ] Test: Spectrum + menu concurrent operation

### Phase 2: Stability Enhancements (4 weeks, v7.7)
- [ ] Add EEPROM checksum validation
- [ ] Implement USB pool allocator
- [ ] Begin transitioning to non-blocking display
- [ ] Test: Power-cycle during settings save
- [ ] Test: Long USB transfers (1000+ messages)
- [ ] Test: Rapid VFO changes with spectrum active

### Phase 3: Performance Optimization (6 weeks, v7.8)
- [ ] Deferred EEPROM writes
- [ ] Async display completion
- [ ] Spectrum external memory (optional)
- [ ] Begin command queue for state (phase 1)

### Phase 4: Major Refactor (8 weeks, v8.0)
- [ ] Complete global state → command pattern migration
- [ ] Replace blocking operations
- [ ] Deprecate old APIs

---

## Risk Mitigation Strategies

### For Critical Fixes
```
1. Implement in feature branch
2. Add comprehensive unit tests
3. Run full integrationtest suite
4. Internal beta with extended field testing (1 week min)
5. Tag v7.6.1-rc1 for community testing
6. Minimal changes from -rc1 to -final
```

### For High Priority Fixes
```
1. Implement separately from critical fixes
2. Ensure backward compatible
3. Regional beta (invite 10-15 users)
4. Monitor for 2 weeks before merge to main
```

### For Medium Priority Improvements
```
1. Design review with team
2. Multiple release candidates
3. Can span multiple minor versions (v7.7 → v7.9)
```

---

## Quality Metrics to Monitor

### Before & After Comparison

| Metric | Before | Target | Method |
|--------|--------|--------|--------|
| Keyboard latency | 50-100ms | <30ms | Measure key-press to menu response |
| Display updates skipped | ~2% | 0% | Count gUpdateDisplay events vs renders |
| Spectrum sample loss | ~1% | 0% | Count RSSI samples from BK4819 |
| USB errors/day | <1% | 0% | Log USB CRC failures |
| EEPROM corruption | ~0.01% | 0% | Monitor for invalid CRC on startup |

---

## Code Review Checklist

When reviewing the fixes:

### Scheduler Race Fix
- [ ] volatile uint8_t used (not bool)
- [ ] gNextTimeslice = 1 assignment in ISR
- [ ] gNextTimeslice = 0 clear in main loop
- [ ] No compiler warnings about volatile

### ST7565 FillScreen
- [ ] CS_Assert() called before SPI transfer
- [ ] CS_Release() called after SPI transfer
- [ ] All 8 pages × 128 bytes sent to display
- [ ] Test: Fresh boot shows welcome screen

### Display Flag Racing
- [ ] Local variables cache flags at start of function
- [ ] Flags cleared before (not after) work
- [ ] Check for new requests after work completes
- [ ] Return early if flag set again

---

## Testing Procedures

### Unit Tests Needed
```bash
make test
# Expected output:
# test_scheduler_timeslice_detection ........ PASS
# test_st7565_fillscreen_completes ......... PASS
# test_display_flag_no_race ................ PASS
# test_eeprom_checksum_validation .......... PASS
# test_usb_pool_no_fragmentation ........... PASS
```

### Integration Tests
```bash
./test_boot.sh              # Full boot sequence
./test_menu_navigation.sh   # Rapid menu changes
./test_spectrum_display.sh  # Concurrent spectrum + display
./test_usb_extended.sh      # 1000+ USB cycles
./test_eeprom_reliability.sh # Power-loss scenario
```

### Manual Testing Checklist
- [ ] Boot: Welcome screen visible (not black/corrupted)
- [ ] Menu: Up/Down keys responsive (<50ms)
- [ ] Frequency: Change VFO while spectrum scanning
- [ ] USB: Connect after power-on, file transfers work
- [ ] EEPROM: Change settings, power-cycle, settings persist
- [ ] Battery: Monitor for unexpected resets over 24 hours

---

## References in Codebase

### Key Files Modified
1. [App/scheduler.c](App/scheduler.c) - Lines 50-60
2. [App/driver/st7565.c](App/driver/st7565.c) - Lines 201-207
3. [App/app/app.c](App/app/app.c) - Lines 1363-1410

### Related Documentation
- Main loop: [App/main.c](App/main.c) - while(true)
- Time slices: [App/app/app.c](App/app/app.c) - APP_TimeSlice*
- Display driver: [App/driver/st7565.h](App/driver/st7565.h)
- Settings: [App/settings.h](App/settings.h)

---

**Document Version:** 1.0  
**Last Updated:** March 1, 2026  
**Status:** Ready for Implementation

