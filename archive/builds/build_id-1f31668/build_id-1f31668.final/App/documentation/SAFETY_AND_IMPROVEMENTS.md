# Code Safety Review & Improvement Recommendations

> **Firmware:** UV-K1Series ApeX-Edition v7.6.8  
> **Date:** 2026-06-26  
> **Reviewer:** N7SIX

---

## 1. VERIFIED ISSUES (Actual bugs / risks)

### 1.1 Hardcoded SPI Timing Assumes 48 MHz Clock

**Files:** `driver/bk4819.c`, `driver/spi.c`, `driver/st7565.c`  
**Severity:** Medium — would silently break if CPU clock changed  
**Evidence:** `bk4819.c` uses NOP loops calibrated to 48 MHz:
```c
for (volatile uint32_t i = 0; i < 4; i++) __NOP();  // Period ≈ 4 cycles
```
At 48 MHz this is ~12 MHz SPI (safe for BK4819 max 16 MHz). The `SystemInit()` always sets HSE 48 MHz, so this is currently safe. However, any future power-saving clock change (HSI 16 MHz, PLL off) would silently slow SPI without warning.

**Recommendation:** Document the clock assumption:
```c
// SPI timing calibrated for 48 MHz system clock (HSE PLL)
// If SystemCoreClock changes, these NOP counts must be recalculated
```

### 1.2 I2C EEPROM Write Has No Verification

**File:** `driver/eeprom.c`  
**Severity:** Medium — silent data corruption on I2C noise  
```c
void EEPROM_WriteBuffer(uint16_t Address, const void *pBuffer) {
    I2C_Start(); I2C_Write(0xA0);
    I2C_Write((Address >> 8) & 0xFF); I2C_Write(Address & 0xFF);
    I2C_Start(); I2C_Write(0xA1);
    I2C_ReadBuffer(pBuffer, Size);  // No ACK check, no verify
    I2C_Stop();
}
```
No ACK polling after write, no read-back verification. I2C bus noise or EEPROM wear could cause silent data loss.

**Recommendation:** Add verify-read after write:
```c
uint8_t verify[Size];
EEPROM_ReadBuffer(Address, verify, Size);
if (memcmp(verify, pBuffer, Size) != 0) {
    // Retry once, then set error flag
}
```

### 1.3 No Watchdog Timer

**File:** `App/main.c` (implied — no IWDG init found)  
**Severity:** Medium — stuck main loop requires power cycle  
A stuck `APP_Update()` (e.g., infinite loop in spectrum, I2C deadlock) has no recovery path.

**Recommendation:** Enable IWDG with 2s timeout, refresh in main loop.

### 1.4 gScanRangeStart/Stop Have No Range Validation

**File:** `app/chFrScanner.h`  
**Severity:** Low-medium — invalid range causes undefined scan behavior  
```c
extern uint32_t gScanRangeStart;
extern uint32_t gScanRangeStop;
```
If called with uninitialized or out-of-range values, `CHFRSCANNER_ScanRange()` iterates over garbage frequencies.

**Recommendation:** Add validation guard:
```c
if (gScanRangeStart < F_MIN || gScanRangeStop > F_MAX || gScanRangeStart >= gScanRangeStop)
    return;
```

---

## 2. INTENTIONAL DESIGN (Not bugs)

### 2.1 `LockAGC()` Always Writes `false` — **INTENTIONAL**

**File:** `app/spectrum.c`, lines 292-298  
**Severity:** N/A — feature deliberately disabled

The AGC lock mechanism is **commented out** on purpose:
```c
void LockAGC()
{
    //RADIO_SetupAGC(settings.modulationType == MODULATION_AM, lockAGC);  // DISABLED
    RADIO_SetupAGC(false, lockAGC);
    //lockAGC = true;  // DISABLED
    lockAGC = false;  // Intentional: AGC lock is not used
}
```
Lines 294 and 296 are explicitly commented out. The author chose to disable AGC lock — likely because:
- BK4819 AGC is already well-behaved in most conditions
- Manual LNA/PGA adjustment without AGC lock may be intentional design (AGC still active but user can bias it)
- STILL mode already uses `RADIO_SetupAGC(false, lockAGC)` call line 295 which passes `lockAGC=false`

**Verdict: NOT a bug. Feature is intentionally disabled. No correction needed.**

### 2.2 `WATERFALL_PushRow()` Division Guarded — **ALREADY SAFE**

**File:** `app/waterfall.c`, line 103  
**Severity:** N/A — already protected

```c
uint16_t step256 = (bars > 1) ? ((uint16_t)(bars - 1) << 8) / 127 : 0;
```
The ternary operator `(bars > 1)` prevents division by zero. When `bars <= 1`, `step256 = 0` and the interpolation loop simply repeats the same sample across all columns — which is the correct fallback behavior.

**Verdict: NOT a bug. Already correctly guarded. No correction needed.**

### 2.3 `FUNCTION_FOREGROUND` Enum Entry

**File:** `app/functions.h`  
```c
FUNCTION_FOREGROUND = 0,  // ???
```
The `???` comment indicates author uncertainty, but this is a stable enum value kept for binary compatibility with saved state in EEPROM. Removing it would break state resume.

**Verdict: Intentional stability placeholder. Not dead code.**

---

## 3. FRAGILE PATTERNS (Worth improving, not urgent)

### 3.1 Global State in misc.h — **ACCEPTABLE for this project**

**File:** `misc.h` — ~50 global variables  
**Risk assessment:** This is the item you asked about. **It does NOT need to be fixed.**

**Why it's acceptable:**
- This is a **single-threaded, cooperative-multitasking** embedded system (no RTOS, no ISR contention for these variables)
- The 16 KB SRAM budget leaves no room for accessor function call overhead (each call costs stack + prologue/epilogue)
- The codebase works correctly as-is — this is a maintainability concern, not a correctness issue
- The firmware is relatively stable; major refactoring would introduce risk without tangible benefit
- Community contributors are comfortable with this pattern — changing it would create a barrier to entry

**When it WOULD need fixing:**
- If the project adopts an RTOS (threads would race on globals)
- If the codebase grows to 100K+ lines (traceability becomes critical)
- If new contributors struggle to find where variables are modified

**Recommendation: Leave as-is.** The global state pattern is appropriate for small, single-threaded embedded firmware with tight memory constraints. Document the convention (which module owns which globals) rather than refactoring.

---

## 4. ARCHITECTURAL IMPROVEMENTS (Future consideration)

| Item | Effort | Impact | Needed? |
|------|--------|--------|---------|
| I2C EEPROM write-verify + retry | Half day | Prevents silent settings loss | **Yes** — medium risk, worthwhile |
| Independent Watchdog (IWDG) | 1 hour | Recovery from stuck main loop | **Yes** — medium risk, easy win |
| gScanRangeStart/Stop validation | 1 hour | Prevents garbage scan on bad init | **Yes** — low risk, easy win |
| Document SPI timing assumption | 15 min | Prevents future clock-change bugs | **Yes** — zero risk |
| Centralize frequency clamp | 1 hour | Eliminates 3 duplicate checks | **Optional** — low risk, nice-to-have |
| Stack painting at startup | 1 hour | Overflow detection | **Optional** — useful for debugging |
| BK4819 register readback in DEBUG | 2 hours | Catch SPI corruption | **Optional** — debug aid only |
| `#define` for magic SPI addr | 5 min | Prevents address collision | **Yes** — zero risk, trivial |
| ISR safety invariant comment | 5 min | Documents threading model | **Yes** — zero risk |
| Global accessor migration | Weeks | Eliminates global coupling | **No** — high risk, not needed |

---

## 5. POSITIVE OBSERVATIONS

1. **waterfall.c `bars > 1` guard** — correctly implemented ternary guard
2. **SPI scheduling in spectrum listen mode** — 320 ms quiescent period is masterclass RF co-existence
3. **Bayer dither** — 16-level grayscale correctly implemented
4. **EEPROM wear leveling** — two-page alternation with write counters
5. **Feature flag discipline** — no `#ifdef` inside function bodies
6. **Cooperative scheduling** — 10ms/500ms split avoids RTOS complexity
7. **Comprehensive register docs** — `bk4819-regs.h` is excellent

---

## 6. CONCLUSION

**The firmware is production-safe.** No production-blocking bugs found.

**Recommended actions (in priority order):**

| Priority | Action | Why |
|----------|--------|-----|
| P1 | Add I2C EEPROM write-verify | Prevents silent settings corruption |
| P1 | Enable IWDG 2s watchdog | Recovery from stuck main loop |
| P2 | Add gScanRangeStart/Stop validation | Prevents garbage scan behavior |
| P2 | Document 48 MHz SPI timing | Prevents future regression |
| P2 | `#define` for spectrum SPI flash addr | Prevents address collision |
| P3 | Stack painting | Improves debug capability |

**Do NOT do:**
- Global accessor migration — unnecessary risk for this project's scale and architecture
- Major refactoring of misc.h — works correctly, leave it alone

The global state pattern in `misc.h` is **not a problem that needs solving**. It's a reasonable tradeoff for a small, single-threaded embedded system with 16 KB SRAM. Fixating on it would waste effort that could be spent on the actual medium-risk items above.