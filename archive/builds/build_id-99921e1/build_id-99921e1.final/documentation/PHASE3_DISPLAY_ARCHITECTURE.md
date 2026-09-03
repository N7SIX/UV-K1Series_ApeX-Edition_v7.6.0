# Phase 3 Design: MDC Display Architecture — Display Space Analysis

**Date**: 2026-08-17  
**Context**: LCD is 128×64 pixels, heavily utilized  
**Goal**: Display MDC RX notifications WITHOUT cluttering existing UI

---

## LCD Layout Analysis

### Physical Dimensions
- **128 pixels wide × 64 pixels tall**
- **8 pages** (gFrameBuffer[0-7]) = 8 bytes × 128 pixels = 8 rows of 8-pixel-tall graphics
- **1 pixel = 1 bit in byte**, so each byte = 8 vertical pixels

### Current Usage

```
gFrameBuffer[0]  Lines  0-7   (8 pixels tall)   → Frequency/Channel display
gFrameBuffer[1]  Lines  8-15  (8 pixels tall)   → VFO A/B + Mode
gFrameBuffer[2]  Lines 16-23  (8 pixels tall)   → RX/TX Frequency
gFrameBuffer[3]  Lines 24-31  (8 pixels tall)   → Center Line (dynamic)
gFrameBuffer[4]  Lines 32-39  (8 pixels tall)   → Center Line (dynamic)
gFrameBuffer[5]  Lines 40-47  (8 pixels tall)   → Center Line (dynamic)
gFrameBuffer[6]  Lines 48-55  (8 pixels tall)   → Center Line (dynamic)
────────────────────────────────────────────────
gStatusLine      Lines 56-63  (8 pixels tall)   → Battery, Signal, Mode, Time
────────────────────────────────────────────────
```

### Center Line Options (gFrameBuffer[3-6])
Currently shows ONE of:
- `CENTER_LINE_NONE`
- `CENTER_LINE_IN_USE`
- `CENTER_LINE_SCAN_PROGRESS`
- `CENTER_LINE_AUDIO_BAR`
- `CENTER_LINE_AUDIO_SCOPE`
- `CENTER_LINE_RSSI`
- `CENTER_LINE_AM_FIX_DATA`
- `CENTER_LINE_DTMF_DEC` ← Inline decode display (4 lines)
- `CENTER_LINE_CHARGE_DATA`
- `CENTER_LINE_BEAM`

### Status Line Usage
Already packed with (from ui/status.c):
- Power Save indicator (8px)
- NOAA/Mode indicators (8px)
- Signal strength gauge (variable)
- Battery percentage + icon (variable)
- RX/TX timer (if enabled)
- Mode indicator (MOD/FM/AM/LSB/USB)
- Transmit status
- **Total: ~120/128 pixels used**

**⚠️ Status bar is FULL** — no room for inline MDC display.

---

## Phase 3 Design Options

### Option 1: Dedicated Center Line Mode ✅ RECOMMENDED
**Pros:**
- Uses existing infrastructure (center_line enum)
- Clear visual separation
- 4 lines of space (32 pixels) available
- Can display multiple lines of info
- Temporary (auto-clears after timeout)

**Cons:**
- Replaces whatever was displaying (RSSI, Audio bar, etc.)
- May interrupt user monitoring

**Implementation:**
```c
enum center_line_t {
    // ... existing options ...
    CENTER_LINE_MDC_ALERT      // NEW: MDC notification display
};
```

**Display Content:**
```
┌─────────────────────────────────────────┐
│ MDC Status from 0x1234                  │  Line 3
│ Arg: 0x23                               │  Line 4
│                                         │  Line 5
│ [Auto-close after 3s or dismiss]        │  Line 6
└─────────────────────────────────────────┘
```

---

### Option 2: Temporary Pop-up (Overlay)
**Pros:**
- Doesn't interrupt existing display
- True modal behavior
- Good for alerts

**Cons:**
- Complex to implement (need rendering order)
- Must erase and restore background
- CPU overhead for double-buffering

**Not recommended for this design.**

---

### Option 3: Status Line Integration (Space-Saving)
**Pros:**
- No visual disruption
- Always visible

**Cons:**
- Status line is FULL (120/128 px used)
- Would require removing something else
- Not prominent enough for emergencies

**Not feasible.**

---

### Option 4: Notification Toast (Bottom Corner)
**Pros:**
- Non-intrusive
- Minimal space (8×16 pixels)
- Can stack multiple messages

**Cons:**
- Requires accurate pixel-level rendering
- Hard to read small font
- Still competes with status bar

**Not practical on 128×64 LCD.**

---

### Option 5: Menu Integration Only (Passive)
**Pros:**
- Zero display clutter
- Always accessible via menu
- Clean integration

**Cons:**
- NOT user-visible when something arrives
- Only polled when user checks menu
- Doesn't trigger audio alerts properly

**Not recommended — loses real-time notification value.**

---

## Recommended Approach: Hybrid Model

### PRIMARY: Dedicated Center Line Mode ✅
**When MDC frame arrives:**

1. **If emergency (0x05, 0x06, 0x07)**:
   - Switch center_line → `CENTER_LINE_MDC_ALERT`
   - Display full modal info:
     ```
     ╔════════════════════╗
     ║ !!! EMERGENCY !!!  ║  (red background)
     ║ From: 0x1234       ║
     ║ Type: Emergency    ║
     ║ [Press ANY to dismiss]
     ╚════════════════════╝
     ```
   - Keep visible until user dismisses
   - Play loud triple-beep
   - Optional: Invert display (red on black) for high visibility

2. **If normal (0x00, 0x01, 0x02, 0x04)**:
   - Switch center_line → `CENTER_LINE_MDC_ALERT` (temporary)
   - Display brief info:
     ```
     MDC Status
     From: 0x1234
     Arg: 0x23
     ```
   - Auto-close after 3 seconds
   - Return to previous center_line mode
   - Play soft beep

3. **If unknown/invalid**:
   - Just play beep
   - No visual display (avoid clutter)

### SECONDARY: Menu Integration
**Add to menu system:**
- `Settings → MDC → Last RX Frame`
  - Shows: Opcode, Unit ID, Argument, Timestamp
  - Shows: Valid CRC? Yes/No
  - Shows: Signal strength estimate

---

## Implementation Architecture

### State Management
```c
typedef struct {
    center_line_t    previous_mode;      /* What was showing before MDC */
    uint32_t         dismiss_time;       /* When to auto-close (0 = permanent) */
    bool             is_emergency;       /* High-priority display? */
} MDC_Display_State_t;

extern MDC_Display_State_t g_MDC_DisplayState;
```

### Integration Points

**1. In mdc_handler.c (Phase 2 code):**
```c
void MDC_Handle_Emergency(uint16_t unit_id, uint8_t arg)
{
    // Save current center_line mode
    g_MDC_DisplayState.previous_mode = center_line;
    g_MDC_DisplayState.is_emergency = true;
    g_MDC_DisplayState.dismiss_time = 0;  /* Permanent until dismiss */
    
    // Switch to MDC alert display
    center_line = CENTER_LINE_MDC_ALERT;
    gUpdateDisplay = true;
    
    // Audio alert (existing Phase 2 code)
    MDC_PlayAlert(3);  /* Emergency */
}

void MDC_Handle_Status(uint16_t unit_id, uint8_t arg)
{
    // Save current center_line mode
    g_MDC_DisplayState.previous_mode = center_line;
    g_MDC_DisplayState.is_emergency = false;
    g_MDC_DisplayState.dismiss_time = gGlobalSysTickCounter + 300;  /* 3s */
    
    // Switch to MDC alert display
    center_line = CENTER_LINE_MDC_ALERT;
    gUpdateDisplay = true;
    
    // Audio alert (existing Phase 2 code)
    MDC_PlayAlert(0);  /* Soft beep */
}
```

**2. In ui/main.c (Phase 3 code):**
```c
#ifdef CENTER_LINE_MDC_ALERT
    case CENTER_LINE_MDC_ALERT:
        UI_DisplayMDCAlert();
        break;
#endif
```

**3. New function in ui/mdc.c:**
```c
void UI_DisplayMDCAlert(void)
{
    // Render g_MDC_LastRxFrame to center_line area (gFrameBuffer[3-6])
    // Use large font for emergency (red/inverted)
    // Use normal font for status (normal display)
    
    // Check timeout (for non-emergency alerts)
    if (!g_MDC_DisplayState.is_emergency &&
        gGlobalSysTickCounter >= g_MDC_DisplayState.dismiss_time)
    {
        // Restore previous center_line mode
        center_line = g_MDC_DisplayState.previous_mode;
        gUpdateDisplay = true;
    }
}

void UI_HandleMDCDismiss(void)
{
    // Called when user presses any key during emergency
    center_line = g_MDC_DisplayState.previous_mode;
    gUpdateDisplay = true;
}
```

---

## Display Examples

### Emergency Alert (4 lines available, gFrameBuffer[3-6])
```
┌────────────────────────────────────────────┐
│ !!! EMERGENCY !!!                          │  (inverted/red)
│ From Unit: 0x1234                          │  (large font)
│ Press ANY to dismiss                       │  
│ Loud beep playing...                       │
└────────────────────────────────────────────┘
```

### Status Alert (3 lines available for info)
```
┌────────────────────────────────────────────┐
│ MDC Status Received                        │
│ From: 0x1234 | Arg: 0x23                  │
│ (Auto-close in 2s...)                      │
└────────────────────────────────────────────┘
```

### Command Alert
```
┌────────────────────────────────────────────┐
│ MDC Command                                │
│ From: 0x5678 | Cmd: 0x04                  │
│ (Auto-close in 2s...)                      │
└────────────────────────────────────────────┘
```

---

## Phase 3 Implementation Breakdown

### Task 1: Add to center_line enum
- [ ] Add `CENTER_LINE_MDC_ALERT` to `App/ui/main.h`

### Task 2: Create display state
- [ ] Add `MDC_Display_State_t` to `App/mdc_handler.h`
- [ ] Initialize in `App/mdc_handler.c`

### Task 3: Create MDC UI display function
- [ ] New file: `App/ui/mdc.c` with `UI_DisplayMDCAlert()`

### Task 4: Integrate into main display loop
- [ ] Modify `App/ui/main.c` UI_DisplayMain()
- [ ] Add case for `CENTER_LINE_MDC_ALERT`

### Task 5: Update Phase 2 handlers
- [ ] Modify handlers in `App/mdc_handler.c` to set center_line

### Task 6: Add menu integration (optional)
- [ ] Add menu item to display last RX in menu system

---

## Advantages of This Approach

✅ **No clutter** — Uses existing center_line infrastructure  
✅ **Clear visual** — 4 lines of dedicated space  
✅ **Non-intrusive** — Auto-restores previous display  
✅ **Emergency priority** — Permanent display until dismissed  
✅ **Minimal code** — Reuses existing rendering framework  
✅ **Backward compatible** — Doesn't break other displays  
✅ **Extensible** — Easy to add status/timer to center_line  
✅ **Battery efficient** — No extra rendering overhead

---

## Summary

**Phase 3 Design Decision: Dedicated Center Line Mode**

This approach leverages the existing `center_line` infrastructure to provide:
1. **Real-time notifications** without cluttering status bar
2. **Emergency modals** with full 4-line display area
3. **Auto-dismissal** for routine frames (3s timeout)
4. **User control** for critical frames (requires manual dismiss)
5. **Clean restoration** of previous display mode

**Not recommended:** Pop-ups, status bar overlay, or inline text — LCD is too small and already full.

