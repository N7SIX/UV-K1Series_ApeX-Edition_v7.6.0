# Recommendation Safety Analysis

> **Question:** Are the safety recommendations safe to implement?  
> **Date:** 2026-06-26

---

## Safety Classification

| Recommendation | Risk Level | Rationale |
|---------------|-----------|-----------|
| Document SPI timing assumption | **ZERO** | Comment-only change |
| ISR safety invariant comment | **ZERO** | Comment-only change |
| `#define SPECTRUM_SETTINGS_SPI_ADDR` | **ZERO** | Pure rename, no behavior change |
| gScanRangeStart/Stop validation | **LOW** | Could reject previously-accepted invalid values |
| Centralize frequency clamp | **LOW** | Must match existing inline logic exactly |
| Stack watermarking / painting | **LOW** | Writes known pattern; no functional change |
| BK4819 readback in DEBUG | **LOW** | Debug-only, zero production impact |
| I2C EEPROM write-verify | **MEDIUM** | Adds latency, Size must be bounded |
| IWDG watchdog | **MEDIUM** | Timeout selection critical |
| Global accessor migration | **HIGH** | Large refactor, easy to miss call sites |

---

## ZERO-RISK (Safe to implement immediately)

### 1. Document SPI Timing Assumption

**Change:** Add comment to NOP loops in bk4819.c, spi.c, st7565.c  
**Risk:** None — comment-only  
**Implementation:**
```c
// Timing calibrated for 48 MHz HSE PLL (SystemCoreClock = 48 MHz)
// 4 NOPs ≈ 4 cycles @ 48 MHz = 83 ns → ~12 MHz SPI clock
// DO NOT change SystemCoreClock without recalculating these values
for (volatile uint32_t i = 0; i < 4; i++) __NOP();
```

### 2. Add ISR Safety Invariant Comment

**Change:** Add block comment to st7565.h and waterfall.h  
**Risk:** None — documentation only  
**Implementation:**
```c
/* INVARIANT: gFrameBuffer and waterfallHistory are ONLY accessed
 * from the main loop (Tick/Render). No ISR, DMA, or nested
 * context reads or writes these buffers. If adding DMA SPI or
 * USB ISR access, you MUST add synchronization. */
```

### 3. Centralize Magic Number

**Change:** Add `#define` to spectrum.h or a shared config header  
**Risk:** Zero — alias only  
**Implementation:**
```c
// In app/spectrum.h or a new app/config.h:
#define SPECTRUM_SETTINGS_SPI_ADDR  0x00A148u
// Update spectrum.c:
PY25Q16_WriteBuffer(SPECTRUM_SETTINGS_SPI_ADDR, ...);
```

---

## LOW-RISK (Safe with basic verification)

### 4. gScanRangeStart/Stop Range Validation

**Current behavior:** If uninitialized (random SRAM values), scan range could be invalid but bounded by loop counters — it would iterate some random frequency range and stop when `i >= MEASUREMENTS_COUNT`. Not catastrophic.

**After change:** Validation aborts scan if range is invalid.  
**Risk:** Minimal — only affects edge case of uninitialized globals.  
**Verification needed:** Confirm `F_MIN` and `F_MAX` are defined at the point of validation (they are in spectrum.c via `frequencies.h`).

**Safe implementation:**
```c
bool CHFRSCANNER_ValidateRange(void) {
    return gScanRangeStart >= F_MIN && 
           gScanRangeStop <= F_MAX && 
           gScanRangeStart < gScanRangeStop;
}
```

### 5. Centralize Frequency Clamp

**Current behavior:** Three separate `if (freq < F_MIN || freq > F_MAX)` checks. All are functionally identical but could theoretically diverge if someone edits one but not others.

**After change:** Single function `FREQUENCIES_Clamp()`.  
**Risk:** Low — but must verify the existing logic:
- `app.c`: `APP_SetFreqByStepAndLimits()` — clamps and returns bool
- `spectrum.c`: `currentFreq += delta; currentFreq = CLAMP(currentFreq);` 
- `radio.c`: `RADIO_ConfigureChannel()` — checks before writing to BK4819

**Verification:** All three currently do `if (freq < F_MIN) freq = F_MIN; if (freq > F_MAX) freq = F_MAX;`. The new function must match exactly.

**Safe implementation:**
```c
// frequencies.c
uint32_t FREQUENCIES_Clamp(uint32_t freq) {
    if (freq < F_MIN) return F_MIN;
    if (freq > F_MAX) return F_MAX;
    return freq;
}
// Then replace each inline clamp with a call.
```

### 6. Stack Watermarking / Painting

**Current behavior:** No overflow detection. Stack grows downward from `_estack`; overflow would corrupt `.bss` or heap silently.

**After change:** Paint bottom 1 KB of stack with `0xCD` at startup. Check at runtime (or on fault).  
**Risk:** Very low — writes to RAM only at startup.  
**Caveat:** Must know the stack size. Linker script defines `_estack` but not `_sstack`/_estack`. In this firmware, SRAM is 16 KB at 0x20000000, so `_estack = 0x20000000 + 16K = 0x20004000`.

**Safe implementation:**
```c
// In main.c after SystemInit():
extern uint32_t _estack;
#define STACK_PAINT_SIZE  0x400  // 1 KB
memset((void*)((uint32_t)&_estack - STACK_PAINT_SIZE), 0xCD, STACK_PAINT_SIZE);
```

### 7. BK4819 Readback in DEBUG

**Current behavior:** Writes are fire-and-forget.

**After change:** In debug builds, read back and compare.  
**Risk:** Zero in production (behind `#ifdef DEBUG`). In debug, adds ~4 µs per register write (one extra SPI read). Negligible for typical use (a few register writes per Tick).  
**Caveat:** Some BK4819 registers are "write-only" or have side effects on read. Must only readback non-destructive registers (REG_13, REG_30, REG_38, etc.). Consult `bk4819-regs.h` for read safety.

**Safe implementation:**
```c
#ifdef DEBUG
    uint16_t readback = BK4819_ReadRegister(reg);
    if (readback != val) {
        // Log error, increment diagnostic counter
        gDebugRegMismatchCount++;
    }
#endif
```

---

## MEDIUM-RISK (Safe with careful implementation)

### 8. I2C EEPROM Write-Verify

**Current behavior:** Write completes without verification.

**After change:** Read back and compare.  
**Risk:** Medium — adds significant latency. An EEPROM write takes ~5-10 ms; adding a full read-back doubles the time. Also:  
- **Size must be bounded** — if `Size` is large (e.g., 256 bytes), verify buffer uses 256 bytes of stack. In a 16 KB SRAM system, this is acceptable but should use static buffer, not stack.  
- **Retry logic could loop forever** — cap at 2 retries.  
- **EEPROM wear** — verify read adds one extra read (no extra write), so no additional wear.

**Safe implementation:**
```c
// Static buffer to avoid stack usage
static uint8_t eeprom_verify_buf[EEPROM_PAGE_SIZE];  // 64 or 128 bytes

void EEPROM_WriteBuffer_Verified(uint16_t Address, const void *pBuffer, uint8_t Size) {
    EEPROM_WriteBuffer(Address, pBuffer, Size);
    if (Size > sizeof(eeprom_verify_buf)) return;  // Can't verify oversized writes
    
    EEPROM_ReadBuffer(Address, eeprom_verify_buf, Size);
    if (memcmp(eeprom_verify_buf, pBuffer, Size) != 0) {
        EEPROM_WriteBuffer(Address, pBuffer, Size);  // One retry
        EEPROM_ReadBuffer(Address, eeprom_verify_buf, Size);
        if (memcmp(eeprom_verify_buf, pBuffer, Size) != 0) {
            gEepromWriteErrorCount++;  // Persistent error flag
        }
    }
}
```

### 9. Independent Watchdog (IWDG)

**Current behavior:** No watchdog. Stuck main loop requires power cycle.

**After change:** IWDG with 2s timeout, refreshed in main loop.  
**Risk:** Medium — timeout selection is critical:
- **Too short:** Triggers during legitimate long operations (EEPROM write 10ms, spectrum sweep 5-10ms, USB enumeration 100ms+). 2 seconds is safe for all operations.
- **Too long:** Defeats the purpose. 2s is standard for this class of device.

**Additional risk:** If `APP_Update()` enters an infinite loop, the system resets. This is the INTENDED behavior — but it means any new infinite-loop bug will cause sudden reboots during testing. This is actually GOOD (fails fast), but developers need to be aware.

**Safe implementation:**
```c
// In board.c or main.c early init:
IWDG_Init();  // 2s timeout, LSI 40 kHz

// In main.c main loop:
while (1) {
    APP_Update();
    IWDG_Reload();  // Pet the watchdog
}
```

**Critical:** The `IWDG_Reload()` must be AFTER `APP_Update()`. If `APP_Update()` takes >2s, the watchdog fires. Currently `APP_Update()` + 10ms timeslice takes ~1-5ms, well within 2s.

---

## HIGH-RISK (Requires extensive testing)

### 10. Global State Accessor Migration

**Current behavior:** Any module can read/write `gEeprom`, `gCurrentFunction`, etc.

**After change:** Centralized accessor functions.  
**Risk:** HIGH — this is a large refactor across many files. Mistake risks:
- Missing a call site — code compiles but uses stale/wrong data
- Thread safety — if accessors add locking in the future, could deadlock
- Performance — accessor function call overhead (negligible on Cortex-M0+ at 48 MHz)

**Safe approach:** Do NOT attempt this in one commit. Instead:
1. Start with one struct field (e.g., `gEeprom.SQUELCH_LEVEL`)
2. Add accessor: `uint8_t SETTINGS_GetSquelchLevel(void)`
3. Update ONE caller at a time, test after each
4. Only proceed if each change passes hardware test

**Verdict:** Worthwhile long-term, but not suitable for quick PR. Requires staged rollout.

---

## Summary: Safe-to-Implement Checklist

| Recommendation | Safe? | Condition |
|---------------|-------|-----------|
| Document SPI timing | ✅ Yes | Comment only |
| ISR safety invariant comment | ✅ Yes | Comment only |
| `#define` for magic SPI addr | ✅ Yes | Pure rename |
| gScanRangeStart/Stop validation | ✅ Yes | gScanRangeStart must be initialized before first use (check callers) |
| Centralize frequency clamp | ✅ Yes | Verify all 3 inline versions are identical before replacing |
| Stack painting | ✅ Yes | Confirm `_estack` symbol exists in linker script |
| BK4819 readback in DEBUG | ✅ Yes | Only read non-destructive registers |
| I2C EEPROM write-verify | ⚠️ Caution | Use static buffer, cap retries, ensure Size ≤ EEPROM page size |
| IWDG watchdog | ⚠️ Caution | 2s timeout tested on all code paths; ensure `APP_Update()` never blocks >2s |
| Global accessors | ❌ Not safe for quick PR | Requires staged rollout with hardware testing at each step |

---

## Final Verdict

**6 recommendations are zero/low risk** and safe to implement immediately with basic review.  
**2 recommendations need careful implementation** (I2C verify, IWDG) but are safe if constraints are respected.  
**1 recommendation is long-term architecture** (global accessors) requiring staged rollout.

The firmware is already production-safe. These recommendations are **defensive improvements**, not correctness fixes.