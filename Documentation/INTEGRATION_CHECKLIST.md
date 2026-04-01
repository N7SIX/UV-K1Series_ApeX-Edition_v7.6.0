<!--
=====================================================================================
Event System Integration Checklist
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# Event System Integration Checklist

Use this document as you integrate the event system into the codebase. Keep track of progress and mark items as completed.

---

## Pre-Integration Setup (Do First)

- [ ] **Read Documentation**
  - [ ] Review `EVENT_SYSTEM_SUMMARY.md` (overview)
  - [ ] Read `EVENT_SYSTEM_QUICKREF.md` (API reference)
  - [ ] Study `EVENT_SYSTEM_GUIDE.md` (architecture)
  - [ ] Review `EVENT_SYSTEM_EXAMPLES.md` (real examples)

- [ ] **Verify Files Exist**
  - [ ] `App/app/events.h` exists (131 lines)
  - [ ] `App/app/events.c` exists (179 lines)
  - [ ] All docs exist in `Documentation/`

- [ ] **Compilation Check**
  - [ ] Run: `./compile-with-docker.sh ApeX`
  - [ ] No new compilation errors
  - [ ] events.c compiles without warnings
  - [ ] Record commit hash: ____________

---

## Phase 1: TX Channel Selection EVENT_SAVE_CHANNEL)

### Overview
Replace `gEeprom.TX_CHANNEL` global assignment patterns with `APP_EVENT_SAVE_CHANNEL` event dispatch.

**Files to modify**: menu.c, settings.c, radio.c, ui.c  
**Estimated effort**: 2-3 days  
**Priority**: HIGH (highest-impact global)

### Step 1: Add Event Type to Enum
- [ ] Open `App/app/events.h`
- [ ] Verify `APP_EVENT_SAVE_CHANNEL` exists in `APP_EventType_t` enum
- [ ] Read the comment description
- [ ] Status: ✅ Already present

### Step 2: Create Event Handler in settings.c

- [ ] Create function in `App/settings.c`:

```c
// Add near top of file, after includes
#include "App/app/events.h"

// Add after other static functions
static void SETTINGS_OnSaveChannel(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint8_t channel_idx = *(const uint8_t *)data;
    
    // Validate
    if (channel_idx >= CHANNELS_MAX) {
        return;
    }
    
    // Save to settings
    gEeprom.TX_CHANNEL = channel_idx;
    SETTINGS_SaveChannel(channel_idx, &gCurrentChannelData);
}
```

- [ ] Verify function compiles
- [ ] Add comment: "Event handler: TX channel changed"

### Step 3: Create Event Handler in radio.c

- [ ] Create function in `App/driver/bk4819.c`:

```c
#include "App/app/events.h"

static void RADIO_OnSaveChannel(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint8_t channel_idx = *(const uint8_t *)data;
    
    // Update radio IC
    RADIO_SetTxChannel(channel_idx);
    BK4819_SetFrequency(gVfoInfo[gTxVfo].Frequency);
}
```

- [ ] Verify function compiles
- [ ] Add comment: "Event handler: TX channel changed"

### Step 4: Create Event Handler in ui.c (or ui/main.c)

- [ ] Create function in UI module:

```c
#include "App/app/events.h"

static void UI_OnChannelChange(APP_EventType_t event, const void *data)
{
    // Trigger display refresh
    gUpdateDisplay = true;
}
```

- [ ] Verify function compiles

### Step 5: Register Handlers in app.c

- [ ] Open `App/app/app.c` (or create if missing)
- [ ] Add function near top:

```c
#include "App/app/events.h"

void APP_Init_EventHandlers(void) {
    // Phase 1: TX Channel Selection
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, SETTINGS_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, RADIO_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, UI_OnChannelChange);
}
```

- [ ] Declare function in `App/app/app.h`:
```c
void APP_Init_EventHandlers(void);
```

- [ ] Verify app.c compiles

### Step 6: Initialize Event System in main.c

- [ ] Open `App/main.c`
- [ ] Add includes at top:
```c
#include "App/app/events.h"
#include "App/app/app.h"
```

- [ ] Find existing `main()` function
- [ ] Add after other init calls (around where `BOARD_Init()` is):
```c
// Initialize event system
APP_EventSystem_Init();
APP_Init_EventHandlers();
```

- [ ] Verify main.c compiles

### Step 7: Update CMakeLists.txt

- [ ] Open `App/CMakeLists.txt`
- [ ] Find the add_executable or target_sources section
- [ ] Verify `App/app/events.c` is listed
- [ ] If not found, add: `App/app/events.c`

### Step 8: Replace Global Assignments in menu.c

- [ ] Search `App/app/menu.c` for: `gEeprom.TX_CHANNEL =`
- [ ] Count occurrences: _______ (record for later verification)
- [ ] For EACH occurrence, replace with:

**Before**:
```c
gEeprom.TX_CHANNEL = new_channel;
// ... maybe some of these
SETTINGS_SaveChannel(new_channel, ...);
RADIO_SetTxChannel(new_channel);
gUpdateDisplay = true;
```

**After**:
```c
APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &new_channel);
```

- [ ] Count replacements: _______ (should match count above)
- [ ] Remove duplicate calls in menu.c (SETTINGS_SaveChannel, RADIO_SetTxChannel, gUpdateDisplay already handled by handlers)

### Step 9: Test Compilation

- [ ] Run: `./compile-with-docker.sh ApeX`
- [ ] Check for errors in menu.c, settings.c, radio.c, app.c, main.c
- [ ] Expected: 0 errors, 0 warnings
- [ ] Status: ✅ PASS / ❌ FAIL (explain): _______________________

### Step 10: Add Debug Logging (Temporary)

- [ ] Add to handlers in settings.c, radio.c, ui.c:
```c
static void SETTINGS_OnSaveChannel(APP_EventType_t event, const void *data)
{
    UART_Printf("DEBUG: SETTINGS_OnSaveChannel called\n");
    // ... rest of handler
}
```

### Step 11: Functional Test

- [ ] Build firmware with Docker
- [ ] Flash to device
- [ ] Physically test TX channel selection:
  - [ ] Menu → Select TX channel
  - [ ] Verify channel changes
  - [ ] Check UART debug output for all 3 handler calls
  - [ ] Verify radio IC updated (frequency display, etc)
  - [ ] Verify display refreshed

### Step 12: Cleanup & Documentation

- [ ] Remove debug logging (UART_Printf calls)
- [ ] Rebuild: `./compile-with-docker.sh ApeX`
- [ ] Add comment in menu.c above event raise:
```c
// Notify interested modules of TX channel change via event system
APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &new_channel);
```

- [ ] Update `App/app/events.h` event enum comment if needed
- [ ] Mark phase 1 complete in documentation

---

## Phase 2: Frequency Change (`APP_EVENT_FREQUENCY_CHANGE`)

**Status**: ⏸ Not Started  
**Estimated effort**: 2-3 days  
**Dependencies**: Phase 1 must pass tests first

### Subtasks
- [ ] Create event enum (already exists)
- [ ] Create handler in radio.c
- [ ] Create handler in ui.c
- [ ] Register handlers
- [ ] Find all `gVfoInfo[gTxVfo].Frequency =` in menu.c
- [ ] Replace with `APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &freq)`
- [ ] Test compilation
- [ ] Functional test on device
- [ ] Add debug logging, test, remove
- [ ] Clean up and document

**Progress Notes**:
_________________________________________________________________________________

---

## Phase 3: Power Level Changes (`APP_EVENT_POWER_CHANGE`)

**Status**: ⏸ Not Started  
**Estimated effort**: 1-2 days  
**Dependencies**: Phase 1-2 should be working

### Subtasks
- [ ] Create event enum (already exists)
- [ ] Find all `gEeprom.PowerLevel =` assignments
- [ ] Create handler(s)
- [ ] Register handlers
- [ ] Replace assignments with event raises
- [ ] Test

**Progress Notes**:
_________________________________________________________________________________

---

## Phase 4: Mode Changes (`APP_EVENT_MODE_CHANGE`)

**Status**: ⏸ Not Started  
**Estimated effort**: 1-2 days

### Subtasks
- [ ] Create event enum (already exists)
- [ ] Find mode-related globals that change
- [ ] Create handlers
- [ ] Register handlers
- [ ] Replace assignments with event raises
- [ ] Test

**Progress Notes**:
_________________________________________________________________________________

---

## Testing Checklist

### Per Phase Unit Tests

**Phase 1 Tests**:
- [ ] Test: Subscribe handler to `APP_EVENT_SAVE_CHANNEL`
- [ ] Test: Raise event with valid channel number
- [ ] Test: Verify handler called
- [ ] Test: Verify gEeprom.TX_CHANNEL updated
- [ ] Test: Verify multiple handlers all called
- [ ] Test: Unsubscribe handler, verify not called on next raise

**Phase 2 Tests**:
- [ ] Test: Frequency change with valid frequency
- [ ] Test: Frequency change with invalid frequency (bounds check)
- [ ] Test: Verify gVfoInfo updated
- [ ] Test: Verify display update triggered

**Phase 3+ Tests**:
- [ ] (Similar pattern for power, mode, etc)

### Integration Test Setup

Create `tests/test_events_integration.cpp`:

```cpp
#include <gtest/gtest.h>
#include "App/app/events.h"
#include "App/settings.h"
#include "App/radio.h"

class EventIntegrationTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        APP_EventSystem_Init();
    }
};

TEST_F(EventIntegrationTest, TxChannelChangeFlow) {
    // Initialize
    APP_EventSystem_Init();
    APP_Init_EventHandlers();
    
    // Initial state
    uint8_t initial = gEeprom.TX_CHANNEL;
    
    // Trigger event (simulating user action)
    uint8_t new_channel = (initial + 1) % CHANNELS_MAX;
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &new_channel);
    
    // Verify state changes
    EXPECT_EQ(new_channel, gEeprom.TX_CHANNEL);
    // Add more assertions as needed
}
```

- [ ] Create test file
- [ ] Add Phase 1 tests
- [ ] Run: `./compile-with-docker.sh ApeX && ctest`
- [ ] All tests pass
- [ ] Add Phase 2 tests after Phase 2 completes

---

## Verification Checkpoints

### After Phase 1
- [ ] Compilation: 0 errors, 0 warnings
- [ ] All 3 handlers subscribed and called (debug log proof)
- [ ] TX channel change works end-to-end
- [ ] No global assignment patterns remaining for TX channel
- [ ] Tests pass

### After Phase 2
- [ ] All Phase 1 still working ✅
- [ ] Frequency change works end-to-end
- [ ] All frequency handlers called
- [ ] No frequency assignment patterns in menu.c
- [ ] New tests pass

### After Phase 3-4
- [ ] All previous phases still working ✅
- [ ] Power/mode changes work
- [ ] All handlers called
- [ ] Tests passing

### Final (All Phases Complete)
- [ ] Full firmware compiles without warnings
- [ ] Device boots and operates normally
- [ ] All features tested on hardware
- [ ] ~80% of problematic globals replaced
- [ ] Documentation updated

---

## Troubleshooting

### Event Not Firing
- [ ] Verify handler is subscribed: Check `APP_SubscribeEvent` call in `APP_Init_EventHandlers()`
- [ ] Verify `APP_EventSystem_Init()` called in main.c
- [ ] Add UART_Printf in emitter to confirm event raise
- [ ] Check handler pointer (not NULL)

### Handler Not Called
- [ ] Confirm subscription worked (check return value)
- [ ] Verify event type matches (enum value)
- [ ] Check handler signature matches `APP_EventCallback_t`
- [ ] Verify data pointer validity

### Compilation Errors
- [ ] Ensure `#include "App/app/events.h"` in all modified files
- [ ] Check handler function signatures match typedef
- [ ] Verify CMakeLists.txt includes events.c
- [ ] Try clean build: `rm -rf build && ./compile-with-docker.sh ApeX`

### Global State Not Updating
- [ ] Verify global write is in handler, not emitter
- [ ] Check handler for validation bugs (early returns)
- [ ] Ensure data is copied, not pointer stored
- [ ] Verify handler is actually called (add debug log)

### Multiple Handlers Not All Called
- [ ] Verify all subscriptions successful (check return values)
- [ ] Check for early return in handler (prevents next handlers)
- [ ] Verify loop in event.c iterates through all subscribers
- [ ] Count handler calls with static counter

---

## Quick Reference

### Key Files
- Implementation: `App/app/events.h`, `App/app/events.c`
- Startup: `App/main.c`, `App/app/app.c`
- Emitters: `App/app/menu.c`, `App/app/uart.c`, etc.
- Handlers: `App/settings.c`, `App/driver/bk4819.c`, `App/ui/`, etc.

### Key Functions
```c
APP_EventSystem_Init()                    // Call in main.c:main()
APP_Init_EventHandlers()                  // Call in main.c:main() after init
APP_SubscribeEvent(event, callback)       // Register handler
APP_UnsubscribeEvent(event, callback)     // Remove handler
APP_RaiseEvent(event, data)               // Emit event
```

### Key Event Types
```c
APP_EVENT_SAVE_CHANNEL      // Phase 1: TX channel selected
APP_EVENT_FREQUENCY_CHANGE  // Phase 2: Frequency modified
APP_EVENT_POWER_CHANGE      // Phase 3: TX power changed
APP_EVENT_MODE_CHANGE       // Phase 4: Operating mode changed
```

---

## Notes & Progress

**Started**: _________________  
**Expected Completion**: _________________  
**Actual Completion**: _________________  

**Key Decisions**:
- _________________________________________________________________________________

**Issues Encountered**:
1. _________________________________________________________________________________
2. _________________________________________________________________________________

**Lessons Learned**:
- _________________________________________________________________________________

**Next Developer Notes**:
- _________________________________________________________________________________

---

**Document Version**: 1.0  
**Last Updated**: 2024  
**Status**: In Progress
