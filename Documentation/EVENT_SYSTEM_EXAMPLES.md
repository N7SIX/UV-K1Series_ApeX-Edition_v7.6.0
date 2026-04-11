<!--
=====================================================================================
Event System Refactoring Examples
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# Event System Refactoring Examples

## Real-World Integration Examples

This document shows concrete **before/after** code for integrating the event system into existing modules.

---

## Example 1: TX Channel Selection (menu.c → settings.c → radio.c)

### Current Problem

**menu.c** (User selects TX channel):
```c
void MENU_SelectTxChannel(uint8_t new_channel) {
    gEeprom.TX_CHANNEL = new_channel;  // ❌ Direct global write
    
    // Implicit dependency: caller must remember to call these
    SETTINGS_SaveChannel(new_channel, &gCurrentChannelData);
    RADIO_SetTxChannel(new_channel);
    gUpdateDisplay = true;  // ❌ Implicit UI state
    gSchedulerUpdate = true;
}
```

**Problem**: Multiple places can modify `gEeprom.TX_CHANNEL` independently, causing inconsistent state.

### Event-Based Solution

**Step 1: Emit Event from menu.c**
```c
// App/app/menu.c
void MENU_SelectTxChannel(uint8_t new_channel) {
    // Just publish the decision, let event handlers manage state
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &new_channel);
}
```

**Step 2: Add Handler in settings.c**
```c
// App/settings.c

// Add to APP_Init_EventHandlers() in app.c:
// APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, SETTINGS_OnSaveChannel);

static void SETTINGS_OnSaveChannel(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint8_t channel_idx = *(const uint8_t *)data;
    
    // Validate channel index
    if (channel_idx >= CHANNELS_MAX) {
        return;  // Silently ignore invalid channel
    }
    
    // Update persistent settings
    gEeprom.TX_CHANNEL = channel_idx;
    SETTINGS_SaveChannel(channel_idx, &gCurrentChannelData);
    
    // Notify dependents of the change
    APP_RaiseEvent(APP_EVENT_CHANNEL_SAVED, &channel_idx);
}
```

**Step 3: Add Handler in radio.c**
```c
// App/radio.c

// Add to APP_Init_EventHandlers():
// APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, RADIO_OnSaveChannel);

static void RADIO_OnSaveChannel(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint8_t channel_idx = *(const uint8_t *)data;
    
    // Update radio IC with new TX channel settings
    RADIO_SetTxChannel(channel_idx);
    RADIO_ApplySettings();
}
```

**Step 4: Update UI Display**
```c
// App/ui/main.c (or similar)

// Add to APP_Init_EventHandlers():
// APP_SubscribeEvent(APP_EVENT_CHANNEL_SAVED, UI_OnChannelSaved);

static void UI_OnChannelSaved(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    // Trigger display refresh
    gUpdateDisplay = true;
}
```

**Result**: 
- ✅ Single source of truth for TX channel change
- ✅ All dependents notified automatically
- ✅ No implicit coupling between modules
- ✅ Easy to add new handlers (e.g., logging, telemetry)

---

## Example 2: Frequency Change (VFO State)

### Current Problem

**In menu.c**:
```c
void AdjustFrequency(int32_t delta) {
    gVfoInfo[gTxVfo].Frequency += delta;
    
    // Inconsistent: sometimes caller forgets this
    RADIO_SetFrequency(gVfoInfo[gTxVfo].Frequency);
    gUpdateDisplay = true;
    gSchedulerUpdate = true;
}
```

### Event-Based Solution

**Step 1: Emit frequency change event**
```c
// App/app/menu.c
void AdjustFrequency(int32_t delta) {
    uint32_t new_freq = gVfoInfo[gTxVfo].Frequency + delta;
    APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &new_freq);
}
```

**Step 2: VFO handler**
```c
// App/radio.c

static void RADIO_OnFrequencyChange(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint32_t new_freq = *(const uint32_t *)data;
    
    // Validate frequency range
    if (!RADIO_IsValidFrequency(new_freq)) {
        return;  // Fail silently - invalid frequency ignored
    }
    
    // Update VFO
    gVfoInfo[gTxVfo].Frequency = new_freq;
    
    // Update radio IC
    RADIO_SetFrequency(new_freq);
    
    // Notify UI of change
    APP_RaiseEvent(APP_EVENT_VFO_UPDATED, &new_freq);
}
```

**Step 3: UI handler**
```c
// App/ui/main.c

static void UI_OnVFOUpdated(APP_EventType_t event, const void *data)
{
    gUpdateDisplay = true;
}
```

---

## Example 3: UART Command Processing

### Current Problem

**uart.c**:
```c
void UART_ProcessCommand(const uint8_t *cmd, uint8_t length) {
    switch (cmd[0]) {
        case CMD_SET_FREQUENCY:
            // Directly modify state - no consistency check
            gVfoInfo[0].Frequency = *(uint32_t *)(cmd + 1);
            break;
        case CMD_SET_TX_POWER:
            gEeprom.PowerLevel = cmd[1];
            // ❌ Inconsistent: radio IC not updated
            break;
    }
}
```

### Event-Based Solution

```c
// App/app/uart.c

void UART_ProcessCommand(const uint8_t *cmd, uint8_t length) {
    if (length < 2) return;
    
    switch (cmd[0]) {
        case CMD_SET_FREQUENCY: {
            if (length < 5) return;  // Need 4 bytes for frequency
            uint32_t freq = *(const uint32_t *)(cmd + 1);
            APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &freq);
            break;
        }
        case CMD_SET_TX_POWER: {
            uint8_t power = cmd[1];
            APP_RaiseEvent(APP_EVENT_POWER_CHANGE, &power);
            break;
        }
    }
}

// Handler (could be in radio.c or settings.c)
static void RADIO_OnPowerChange(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;
    
    uint8_t power = *(const uint8_t *)data;
    
    // Validate
    if (power >= POWER_LEVEL_MAX) {
        return;  // Ignore invalid power
    }
    
    // Update both settings and radio IC
    gEeprom.PowerLevel = power;
    RADIO_SetPowerLevel(power);
}
```

---

## Integration Checklist For a Module

When adding the event system to an existing module:

### Before You Start
- [ ] Identify all global variables modified by this module
- [ ] List all modules that depend on those globals
- [ ] Check for implicit side effects (UI updates, EEPROM writes)

### Create Events
- [ ] Add event type(s) to `APP_EventType_t` enum
- [ ] Document event data format in comment
- [ ] List expected subscribers

### Refactor Emitter
- [ ] Replace all global assignments with `APP_RaiseEvent()`
- [ ] Remove implicit side effects (gUpdateDisplay, etc.)
- [ ] Test that event is raised (add debug logging)

### Add Handlers
- [ ] Create handler function(s) in dependent modules
- [ ] Copy event data (don't rely on pointer lifetime)
- [ ] Keep handlers focused and side-effect-minimal
- [ ] Register handlers in `APP_Init_EventHandlers()`

### Validate
- [ ] Unit test each handler in isolation
- [ ] Integration test the full chain
- [ ] Check for event loops or circular dependencies
- [ ] Add debug logging temporarily

---

## Code Template: Adding a New Event

Use this template when adding a new event type:

### 1. Declare Event in events.h
```c
typedef enum {
    // ...existing events...
    APP_EVENT_MY_ACTION,  /**< Brief description of when/why this fires */
    _APP_EVENT_MAX
} APP_EventType_t;
```

### 2. Create Emitter Function
```c
// In some module (e.g., menu.c, uart.c)

void MyModule_DoSomething(int param) {
    int payload = param;
    
    // Emit event to notify all interested parties
    APP_RaiseEvent(APP_EVENT_MY_ACTION, &payload);
    
    // Do local work, but don't modify global state
    // (that's for event handlers)
}
```

### 3. Create Handler Template
```c
// In dependent module (could be settings.c, radio.c, ui.c, etc.)

static void MyModule_OnMyAction(APP_EventType_t event, const void *data)
{
    if (data == NULL) return;  // Always validate!
    
    int param = *(const int *)data;
    
    // Validate
    if (param < 0) return;
    
    // Do work specific to this module
    // ...
    
    // Optionally raise follow-up events
    // APP_RaiseEvent(APP_EVENT_NOTIFY_UPSTREAM, &result);
}
```

### 4. Register Handler
```c
// In app.c, somewhere in APP_Init_EventHandlers():

APP_SubscribeEvent(APP_EVENT_MY_ACTION, MyModule_OnMyAction);
```

---

## Testing Template

```c
// In tests/test_events_integration.cpp

#include "App/app/events.h"
#include "gmock/gmock.h"

class EventIntegrationTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        APP_EventSystem_Init();
    }
};

TEST_F(EventIntegrationTest, TxChannelChangeTestPropagation) {
    // Track which modules responded
    bool settings_updated = false;
    bool radio_updated = false;
    bool ui_updated = false;
    
    // Register mocks
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, 
        [&](APP_EventType_t e, const void *d) { settings_updated = true; });
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL,
        [&](APP_EventType_t e, const void *d) { radio_updated = true; });
    
    // Emit event
    uint8_t new_channel = 5;
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &new_channel);
    
    // Verify all handlers fired
    EXPECT_TRUE(settings_updated);
    EXPECT_TRUE(radio_updated);
}

TEST_F(EventIntegrationTest, FrequencyChangeValidation) {
    static uint32_t last_freq = 0;
    
    APP_SubscribeEvent(APP_EVENT_FREQUENCY_CHANGE,
        [&](APP_EventType_t e, const void *d) {
            if (d) {
                uint32_t freq = *(const uint32_t *)d;
                if (freq >= 100000000UL && freq <= 1000000000UL) {
                    last_freq = freq;
                }
            }
        });
    
    // Valid frequency
    uint32_t valid_freq = 146500000UL;
    APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &valid_freq);
    EXPECT_EQ(valid_freq, last_freq);
    
    // Invalid frequency (too low)
    uint32_t invalid_freq = 50000000UL;
    APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &invalid_freq);
    EXPECT_NE(invalid_freq, last_freq);  // Should not update
}
```

---

## Migrating Existing Code: Tips & Tricks

### Use git diff to track changes
```bash
git diff App/app/menu.c
# Before: 50 lines with global assignments
# After: 5 lines with APP_RaiseEvent calls
```

### Gradual Migration Strategy
1. Add event system and new handlers (both run in parallel)
2. Replace one emitter (e.g., TX_CHANNEL in menu.c)
3. Test thoroughly
4. Replace next emitter
5. Remove old global assignment patterns

### Debugging Event Flow
```c
#define DEBUG_EVENTS 1

#ifdef DEBUG_EVENTS
static void debug_log_event(APP_EventType_t event, const void *data) {
    const char *event_names[] = {
        "SAVE_CHANNEL",
        "SAVE_VFO",
        "FREQUENCY_CHANGE",
        "MODE_CHANGE",
        "POWER_CHANGE",
    };
    
    if (event < _APP_EVENT_MAX) {
        UART_Printf("EVENT: %s (%p)\n", event_names[event], data);
    }
}

// Register in app.c
APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, debug_log_event);
// ... subscribe to all event types
#endif
```

---

## Common Patterns Refactoring Summary

| Old Pattern | New Pattern | Benefits |
|-------------|-------------|----------|
| `gGlobal = value;` | `APP_RaiseEvent(EVENT, &value);` | Discoverable, testable |
| Implicit UI update | Explicit `APP_RaiseEvent(EVENT_UI_UPDATE)` | Clear dependencies |
| Cascading function calls | Event handlers | Loose coupling |
| Global flag `gUpdate = true` | `APP_RaiseEvent(EVENT_NEEDS_UPDATE)` | Explicit state transitions |
| Direct EEPROM write | Event → handler writes | Consistent state |

---

## References

- [Event System Header](../App/app/events.h)
- [Event System Implementation](../App/app/events.c)
- [Architecture Guide](./ARCHITECTURE_AUDIT_REPORT.md)
- [Implementation Guide](./IMPLEMENTATION_GUIDE.md)
