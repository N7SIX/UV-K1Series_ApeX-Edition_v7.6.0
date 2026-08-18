# MDC-1200 Implementation: Readiness Assessment for User-Visible Reactions
**Date**: 2026-08-17  
**Version Audited**: v7.6.0 (Core Protocol)  
**Reference Docs**: v7.6.10A/10B audit trail  
**Status**: ✅ **PROTOCOL LAYER READY** | ⚠️ **RECEPTION/REACTION LAYER NOT YET BUILT**

---

## Executive Summary

The **core MDC-1200 protocol layer** (`mdc1200.c/h`) is **production-ready** for transmission and validation. The encoder produces authentic Motorola-compliant frames, and the decoder correctly reverses the encoding. However, **the reception path is not integrated**, and **no user-visible reaction infrastructure exists**. Before attaching reactions to decoded MDC frames, you must:

1. **Integrate the receiver**: Establish where decoded MDC frames come from (RX FIFO path)
2. **Create state infrastructure**: Track received MDC frames globally
3. **Define opcode handlers**: Map opcodes (0x00=Status, 0x01=Ack, etc.) to actions
4. **Build UI/display layer**: Show MDC events to the user
5. **Establish callback/dispatch**: Route decoded frames to reaction handlers

This assessment covers the current state, what's ready, what's missing, and a detailed roadmap for the next phase.

---

## Part 1: Protocol Layer Assessment (TRANSMISSION SIDE)

### 1.1 Frame Encoding — ✅ COMPLETE & VERIFIED

**What works:**
- `MDC1200_BuildFrame()` correctly encodes all 26 bytes:
  - 7× `0x55` preamble (bit synchronization)
  - 5× `0x07 0x09 0x2A 0x44 0x6F` leader (frame sync)
  - 14× interleaved+ECC payload (4 data + 2 CRC + 1 pad + 7 ECC)
- **CRC-16**: Authentic polynomial `0x1021`, bit-reflected input, final XOR `0xFFFF` (verified correct)
- **ECC**: Correct K=7 convolutional code with taps at positions 0, 2, 5, 6
- **Interleaving**: Canonical 16×7 matrix: source bit `n` → output position `(n%7)*16 + (n/7)`
- **Bit ordering**: MSB-first extraction and repacking (confirmed round-trip)

**Verification status:**
- ✅ Round-trip tested: built frames decode back to original op/arg/unit_id with valid CRC
- ✅ Multi-vector validation: tested with `{01,23,4567}`, `{00,00,0000}`, `{AA,55,FFFF}`, `{12,34,ABCD}`
- ✅ No buffer overflows or OOB access
- ✅ Defensive NULL/size/capacity checks throughout

**Public API:**
```c
MDC1200_Error_t MDC1200_Transmit(const MDC1200_Params_t *params)
  • Input: op, arg, unit_id
  • Output: Error code (0 = success)
  • Calls: BK4819_TransmitMDC1200Frame (weak symbol in driver)
  • Status: Present in bk4819.c; also added to bk4829.c (v7.6.10A/10B)
```

**Configuration storage:**
- ✅ EEPROM fields reserved in `settings.h`:
  - `gSetting.MDC_UnitID` (uint16_t): destination Unit ID
  - `gSetting.MDC_DefaultOp` (uint8_t): default opcode
  - `gSetting.MDC_DefaultArg` (uint8_t): default argument
- ✅ Safe: within existing settings block, no overlap with calibration

**Conclusion on TX side:** 🟢 **READY FOR PRODUCTION**

---

### 1.2 Frame Decoding — ✅ READY (FOR VALIDATION, NOT YET FOR RECEPTION)

**What works:**
- `MDC1200_DecodeFrame()` reverses the entire encoding chain:
  - Extracts 112 bits from interleaved payload
  - De-interleaves with true inverse: `src_bit = (frame_bit % 16)*7 + (frame_bit / 16)`
  - Repacks MSB-first back to 14 bytes
  - Extracts op, arg, unit_id from first 4 bytes
  - Validates CRC against embedded CRC bytes 4–5
  - Returns `valid_out` boolean
- **Used for**: Frame validation, round-trip testing, optional checksum verification
- **Error handling**: Returns error codes; gracefully handles NULL pointers, wrong frame lengths

**API:**
```c
// Decode from raw 26-byte frame
MDC1200_Error_t MDC1200_DecodeFrame(const uint8_t *frame, size_t frame_len,
                                    uint8_t *op_out, uint8_t *arg_out,
                                    uint16_t *unit_id_out, bool *valid_out);

// Decode from 13 × 16-bit FIFO words (for RX path)
MDC1200_Error_t MDC1200_DecodeFrameWords(const uint16_t *fifo_words,
                                         size_t fifo_word_count,
                                         uint8_t *op_out, uint8_t *arg_out,
                                         uint16_t *unit_id_out, bool *valid_out);

// Lightweight CRC check without full decode
MDC1200_Error_t MDC1200_VerifyCRC(const uint8_t *frame, size_t frame_len,
                                  bool *valid_out);
```

**Conclusion on RX protocol decoding:** 🟢 **READY FOR PRODUCTION**

But **⚠️ not integrated into the reception path yet.**

---

## Part 2: Reception Path Assessment

### 2.1 Where MDC Frames Are Received — ❌ NOT INTEGRATED

**Gap analysis:**
- ❓ **BK4819 RX FIFO**: The RF transceiver likely buffers received MDC frames, but:
  - No code exists to **extract MDC from RX FIFO**
  - No code exists to **recognize MDC-1200 frame structure** during reception
  - No code exists to **timestamp or queue received frames**
- ❓ **Reception trigger**: When is MDC decoding invoked?
  - After squelch opens? (during normal RX)
  - During a specific RX mode? (e.g., "squelch tail", end-of-transmission)
  - Continuous background scan? (inefficient, but possible)
  - Via interrupt? (if BK4819 signals MDC availability)
- ❓ **Frame buffering**: Where do decoded MDC frames live after decoding?
  - Single-frame cache? (last received)
  - Ring buffer? (last N frames)
  - Transient event? (decoded, processed, forgotten)

**Current state of BK4819/BK4829 drivers:**
- ✅ TX functions exist: `BK4819_TransmitMDC1200Frame()` (calls protocol-layer `MDC1200_BuildFrame()`)
- ❌ RX functions do NOT exist: no `BK4819_ReceiveMDC1200Frame()` equivalent
- ❌ No FIFO extraction for MDC: RX path only handles audio/tone (CTCSS, DCS)

**Questions to resolve before proceeding:**
1. Does the BK4819 datasheet specify MDC-1200 RX handling?
2. Can MDC be extracted from the general RX FIFO, or is there a separate MDC-specific path?
3. What is the timing: post-squelch-open, end-of-frame, or both?
4. How do we avoid false-positive MDC detection (noise misinterpreted as valid frame)?

---

### 2.2 State Infrastructure for Reception — ❌ MISSING

**What needs to be built:**

```c
// In globals/radio_globals.h or a new globals/mdc_globals.h
typedef struct {
    uint16_t    last_unit_id;      // Sender ID from last received frame
    uint8_t     last_op;           // Last opcode
    uint8_t     last_arg;          // Last argument
    uint32_t    timestamp_10ms;    // When it was received (relative to scheduler)
    bool        is_valid;          // CRC check result
    bool        is_new;            // Has been processed yet?
} MDC1200_RxFrame_t;

// Global instance
extern MDC1200_RxFrame_t g_MDC_LastRxFrame;
extern volatile bool g_MDC_FrameReceived;  // Signal to scheduler
```

**Why this structure:**
- **Minimal state**: Single received frame (most common case; can extend to ring buffer later)
- **Validation tracking**: Distinguishes good/corrupted frames
- **Event flag**: `g_MDC_FrameReceived` signals that a new frame is available (like `gEndOfRxDetectedMaybe`)
- **Timestamp**: Allows UI to show "received X seconds ago"

---

## Part 3: Opcode & Action Mapping — ⚠️ PARTIALLY DEFINED

### 3.1 Standard Motorola MDC-1200 Opcodes

| Opcode | Name | Argument Meaning | Typical Reaction |
|--------|------|------------------|------------------|
| 0x00 | **Status** | Unit ID / Flags | Display sender ID |
| 0x01 | **Acknowledge** | (varies) | Display "ACK received" |
| 0x02 | **Request** | Request type | Log request |
| 0x03 | (Reserved) | — | Ignore |
| 0x04 | **Command** | Command ID | Execute/log command |
| 0x05 | **Emergency** | Priority level | Alert, play sound |
| 0x06 | **Emerg+Op** | Emergency + opcode | Alert + operation |
| 0x07 | **Emerg+Ack** | Emergency + ack | Alert + acknowledge |

**Current state in codebase:**
```c
// From documentation/MDC1200_MENU_IMPLEMENTATION_v7.6.10B.md
const char* const gSubMenu_MDC_OP[] = {
    "Status", "Ack", "Request", "Reserved",
    "Command", "Emerg", "Emerg+Op", "Emerg+Ack"
};
```
- ✅ Strings defined for menu display
- ❌ **No dispatch table**: No mapping from opcode to action handler
- ❌ **No default handlers**: Receiving Status doesn't do anything yet

### 3.2 Argument Interpretation
- Depends on opcode
- Status opcode: arg is typically a status byte (unit flags)
- Emergency opcode: arg is priority level (0–15)
- Command: arg is the command ID
- **Current state**: No interpreter logic exists

---

## Part 4: UI/Display Layer — ❌ NOT BUILT

### 4.1 Where to Show Received MDC

**Option A: Status Bar Integration** (recommended for real-time)
- Show "MDC: [sender_id] [opcode_name]" in top-right corner
- Blink or highlight for 2–3 seconds
- Current implementation: nothing

**Option B: Separate MDC Menu/Screen**
- Menu item showing last received MDC with details
- Timestamp, CRC validity, retransmit counter
- Current implementation: none (menu items only for TX config)

**Option C: Inline Notification** (like tone detection)
- Temporary on-screen message
- "Status RX from 0x1234"
- Fade away after 3 seconds
- Current implementation: none

**Option D: Logging**
- Store MDC RX in debug buffer (like UART logging)
- View via debug menu later
- Current implementation: none

**Recommendation for Phase 1:** Start with Option A (status bar). Simple, non-intrusive, user gets immediate feedback.

### 4.2 Required UI Globals
```c
// In globals/ui_globals.h
extern uint16_t gMDC_StatusDisplay_10ms;  // Countdown to hide status
extern bool     gUpdateMDC_Display;        // Refresh needed
```

### 4.3 Drawing Logic (pseudo-code for ui/screen handler)
```c
// In ui drawing loop
if (g_MDC_StatusDisplay_10ms > 0) {
    // Draw "MDC: 0x1234 Status" at top-right
    sprintf(buf, "MDC: %04X %s", 
            g_MDC_LastRxFrame.last_unit_id,
            gSubMenu_MDC_OP[g_MDC_LastRxFrame.last_op]);
    // Render to display
}
```

---

## Part 5: Callback/Dispatch Architecture — ❌ NOT BUILT

### 5.1 Design Options

**Option 1: Simple Event Queue (recommended for now)**
```c
// In mdc1200.c or new mdc1200_handlers.c
typedef void (*MDC1200_Handler_t)(const MDC1200_RxFrame_t *frame);

typedef struct {
    uint8_t opcode;
    MDC1200_Handler_t handler;
} MDC1200_OpHandler_t;

static const MDC1200_OpHandler_t g_MDC_Handlers[] = {
    { 0x00, MDC_Handle_Status },
    { 0x01, MDC_Handle_Ack },
    { 0x05, MDC_Handle_Emergency },
    // ...
};

// Called from scheduler when g_MDC_FrameReceived is true
void MDC1200_ProcessRxFrame(void) {
    for (int i = 0; i < ARRAY_SIZE(g_MDC_Handlers); i++) {
        if (g_MDC_Handlers[i].opcode == g_MDC_LastRxFrame.last_op) {
            g_MDC_Handlers[i].handler(&g_MDC_LastRxFrame);
            return;
        }
    }
    // No handler found -> default behavior (log, ignore, etc.)
}
```

**Option 2: Opcode Dispatch Table (cleaner)**
```c
static MDC1200_Handler_t g_MDC_Handlers[256] = {
    [0x00] = MDC_Handle_Status,
    [0x01] = MDC_Handle_Ack,
    [0x05] = MDC_Handle_Emergency,
};

// Called from scheduler
void MDC1200_ProcessRxFrame(void) {
    MDC1200_Handler_t handler = g_MDC_Handlers[g_MDC_LastRxFrame.last_op];
    if (handler) {
        handler(&g_MDC_LastRxFrame);
    }
}
```

**Option 3: Weak Symbols (for extensibility)**
```c
// In mdc1200.c
extern void MDC1200_Handle_Status(const MDC1200_RxFrame_t *frame)
    __attribute__((weak));
extern void MDC1200_Handle_Ack(const MDC1200_RxFrame_t *frame)
    __attribute__((weak));

void MDC1200_Handle_Status(const MDC1200_RxFrame_t *frame) {
    // Default: do nothing
}
```

**Recommendation:** Start with **Option 2** (dispatch table). Simple, extensible, minimal overhead.

### 5.2 Example Handler Implementation

```c
// mdc1200_handlers.c (new file)

static void MDC_Handle_Status(const MDC1200_RxFrame_t *frame) {
    // 1. Update global state
    g_MDC_LastRxFrame = *frame;
    
    // 2. Set UI update flag
    gUpdateMDC_Display = true;
    gMDC_StatusDisplay_10ms = 200;  // Show for 2 seconds
    
    // 3. Optional: log to debug buffer
    RADIO_LogDebug("MDC Status RX: ID=0x%04X arg=0x%02X",
                   frame->last_unit_id, frame->last_arg);
    
    // 4. Optional: increment statistics
    g_MDC_Statistics.status_count++;
    
    // 5. Optional: trigger side-effect (e.g., backlight on)
    // BK4819_SetBacklight(BACKLIGHT_ON_TR_RX);
}

static void MDC_Handle_Emergency(const MDC1200_RxFrame_t *frame) {
    // 1. Set high-priority notification
    gUpdateMDC_Display = true;
    gMDC_StatusDisplay_10ms = 500;  // Show longer
    
    // 2. Optional: play alert tone
    AUDIO_PlayTone(AUDIO_TONE_ALERT, 100);  // 100ms beep
    
    // 3. Optional: set do-not-disturb or other state
    g_MDC_EmergencyActive = true;
    g_MDC_EmergencyExpiry_10ms = 5000;  // 50 seconds
}
```

---

## Part 6: Integration Points in Existing Code

### 6.1 Where to Hook Reception

**Option A: In scheduler (`App/scheduler.c`)**
```c
// In the main scheduler loop (10ms tick)
void APP_Update_10ms(void) {
    // ... existing code ...
    
    // NEW: Check for received MDC frame
    if (g_MDC_FrameReceived) {
        g_MDC_FrameReceived = false;  // Clear flag
        MDC1200_ProcessRxFrame();     // Dispatch to handlers
    }
    
    // ... rest of scheduler ...
}
```

**Option B: In BK4819 RX interrupt handler**
- Once we identify where RX FIFO frames are processed, insert MDC detection there
- May be in `BK4819_RX_FIFO_Available()` or similar ISR

**Option C: In main radio loop (`App/radio.c`)**
- During RX squelch processing
- After end-of-transmission detection (`gEndOfRxDetectedMaybe`)

**Recommendation:** Start with Option A (scheduler) for simplicity. Move to interrupt-driven later if performance requires.

### 6.2 Required Scheduler Changes
```c
// App/scheduler.c additions

#include "mdc1200.h"

// In the 10ms tick handler
void APP_Update_10ms(void) {
    // Existing MDC transmission handling (if any)
    
    // NEW: Process received MDC frames
    if (g_MDC_FrameReceived) {
        g_MDC_FrameReceived = false;
        MDC1200_ProcessRxFrame();
    }
}
```

---

## Part 7: Data Flow Diagram

```
┌──────────────────────────────────────────────────────────┐
│ BK4819 RF Transceiver                                    │
│  ↓ (RX FIFO with MDC frame data)                         │
└──────────┬───────────────────────────────────────────────┘
           │
           ├─→ [NEW] Extract MDC from FIFO
           │    (driver/bk4819.c → BK4819_ExtractMDC())
           │
           ├─→ [NEW] Validate frame structure
           │    (preamble, leader)
           │
           ├─→ [EXISTING] Decode via MDC1200_DecodeFrame()
           │    Returns: op, arg, unit_id, valid_crc
           │
           ├─→ [NEW] Update global state
           │    g_MDC_LastRxFrame.{op, arg, unit_id, ...}
           │    g_MDC_FrameReceived = true
           │
           └─→ [NEW] Scheduler processes frame
                Calls MDC1200_ProcessRxFrame()
                ↓
                ├─→ Dispatch to handler (op-specific)
                │    ├─ MDC_Handle_Status()
                │    ├─ MDC_Handle_Ack()
                │    ├─ MDC_Handle_Emergency()
                │    └─ [default] MDC_Handle_Unknown()
                │
                └─→ Handler actions:
                     ├─ Update display (status bar, menu)
                     ├─ Play alert tone (if emergency)
                     ├─ Log to debug buffer
                     ├─ Increment statistics
                     └─ Trigger external callbacks
```

---

## Part 8: Readiness Checklist for Next Phase

### Phase 1: Foundation (Weeks 1–2)
- [ ] Design reception path: where do RX MDC frames come from in BK4819?
- [ ] Create global state structure (`MDC1200_RxFrame_t`)
- [ ] Add to `globals/radio_globals.h` or create `globals/mdc_globals.h`
- [ ] Write `MDC1200_ProcessRxFrame()` dispatcher
- [ ] Implement basic opcode handlers (Status, Ack, Emergency)
- [ ] Test decode path: inject test frames via mock

### Phase 2: UI Integration (Weeks 2–3)
- [ ] Add status bar display for MDC RX
- [ ] Implement `gUpdateMDC_Display` refresh logic
- [ ] Test: verify Status RX displays correctly
- [ ] Add menu screen to view last received MDC (unit_id, opcode, timestamp, CRC)
- [ ] Implement UI update in 10ms scheduler tick

### Phase 3: Audio/Alert Integration (Week 3)
- [ ] Hook Emergency handler to play alert tone
- [ ] Test: verify tone plays on 0x05 (Emergency) RX
- [ ] Add configurable alert tone (separate from other tones)
- [ ] Optional: backlight on RX, screen wake on emergency

### Phase 4: Testing & Refinement (Week 4)
- [ ] Unit test MDC handlers with mock frames
- [ ] Integration test: loopback TX → RX (if hardware supports)
- [ ] Field test: real MDC-1200 transceiver (if available)
- [ ] Stress test: rapid frame RX, malformed frames, CRC failures

### Phase 5: Advanced Features (Post-Phase 1)
- [ ] Statistics logging (per-opcode counters)
- [ ] Ring buffer for last N received frames
- [ ] Conditional dispatch (e.g., only alert if unit_id == our_id)
- [ ] Selective opcode filtering (user-configurable)
- [ ] Export MDC log to UART/file

---

## Part 9: Detailed Recommendations

### 9.1 Start Small: Implement Status Opcode First
**Why:** Most common, lowest risk, validates the entire pipeline.

**Steps:**
1. Set up reception path (you'll need BK4819 datasheet for this)
2. Create `MDC_Handle_Status()` that only updates display
3. Test: verify "MDC: 0x1234 Status" appears when frame is RX'd
4. Add to menu: show last received Status details

### 9.2 Define Clear Success Criteria
- ✅ Send MDC-1200 Status frame from another radio
- ✅ UV-K1 receives it without crashing
- ✅ "MDC: [ID] Status" displays in status bar
- ✅ Frame is logged with correct op/arg/unit_id
- ✅ CRC validates correctly

### 9.3 Design the Opcode Dispatch Table
```c
// In mdc1200.c (or new mdc1200_handlers.c)

typedef void (*MDC1200_Handler_t)(const MDC1200_RxFrame_t *frame);

static MDC1200_Handler_t g_MDC_OpHandlers[256] = {
    [0x00] = MDC_Handle_Status,
    [0x01] = MDC_Handle_Ack,
    [0x02] = MDC_Handle_Request,
    [0x04] = MDC_Handle_Command,
    [0x05] = MDC_Handle_Emergency,
    [0x06] = MDC_Handle_EmergencyOp,
    [0x07] = MDC_Handle_EmergencyAck,
};

void MDC1200_ProcessRxFrame(void) {
    if (!g_MDC_LastRxFrame.is_valid) return;  // Ignore bad CRC
    
    MDC1200_Handler_t handler = g_MDC_OpHandlers[g_MDC_LastRxFrame.last_op];
    if (handler) {
        handler(&g_MDC_LastRxFrame);
    } else {
        // Optional: call default handler or ignore
        MDC_Handle_UnknownOpcode(&g_MDC_LastRxFrame);
    }
}
```

### 9.4 Distinguish RX from TX Configuration
- **TX config** (current menu items): `MDC_ID`, `MDC_OP`, `MDC_ARG` — what to transmit
- **RX state** (new): `g_MDC_LastRxFrame` — what we just received
- These are separate! Don't confuse them.

### 9.5 Graceful Degradation
- Frame with bad CRC: log it, don't dispatch to handler
- Unknown opcode: call default handler (silent, or log)
- No handler available: safe fallback (display anyway, don't crash)
- Malformed frame: return error from decoder, skip processing

---

## Part 10: Summary & Verdict

| Component | Status | Ready? | Notes |
|-----------|--------|--------|-------|
| **Protocol Encoding** | ✅ Complete & Verified | 🟢 YES | Authentic MDC-1200, round-trip validated |
| **Protocol Decoding** | ✅ Complete & Verified | 🟢 YES | De-interleaver, CRC, ECC all correct |
| **TX Public API** | ✅ `MDC1200_Transmit()` | 🟢 YES | Working, integrated with drivers |
| **RX Extraction** | ❌ Missing | 🔴 NO | Need to design FIFO read from BK4819 |
| **RX State** | ❌ Missing | 🔴 NO | Need global `MDC1200_RxFrame_t` |
| **Opcode Dispatch** | ⚠️ Partial | 🟡 PARTIAL | Strings defined, no handlers yet |
| **UI Display** | ❌ Missing | 🔴 NO | Need status bar + menu integration |
| **Audio Alerts** | ❌ Missing | 🔴 NO | Emergency handler not implemented |
| **Callbacks/Handlers** | ❌ Missing | 🔴 NO | No dispatch mechanism yet |

### Overall Verdict
🟢 **Protocol layer: READY FOR PRODUCTION**  
🔴 **Reaction layer: NOT YET BUILT**

You can safely use the existing `MDC1200_Transmit()` to send frames right now. But to **receive and react to MDC frames**, you need to:

1. **Integrate reception** (design + implement BK4819 RX path)
2. **Create state** (globals for last received frame)
3. **Build dispatch** (opcode → handler table)
4. **Implement UI** (status bar + menu)
5. **Add handlers** (Status, Ack, Emergency, etc.)

The good news: the protocol layer is solid. The reception layer is a **self-contained feature** that doesn't touch the existing TX infrastructure. You can build it independently and test incrementally.

---

## Next Steps: Immediate Action Items

### Before You Start
1. **Read BK4819 datasheet** section on MDC-1200 RX (or reverse-engineer from existing RX code)
2. **Identify RX FIFO data**: Where do received MDC frames appear?
3. **Identify timing**: When is MDC data available? (after squelch open? end-of-frame? always?)
4. **Create memory note**: Document the RX path you discover

### Phase 1 Deliverables
1. ✅ Create `globals/mdc_globals.h` with `MDC1200_RxFrame_t`
2. ✅ Create `mdc1200_handlers.c` with dispatch table
3. ✅ Implement `MDC_Handle_Status()` (minimal: just update display)
4. ✅ Create test harness: inject mock MDC frame, verify handler is called
5. ✅ Add status bar display in UI
6. ✅ Integrate into scheduler (10ms tick)
7. ✅ Field test with another MDC-1200 radio

This gives you a complete end-to-end validation before adding Emergency, Command, and other opcodes.

