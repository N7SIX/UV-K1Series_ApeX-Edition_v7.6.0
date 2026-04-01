<!--
=====================================================================================
Event System Integration Guide
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# Event System Integration Guide

## Overview

The **Application Event System** (`App/app/events.h/c`) provides a callback-based architecture to decouple modules and replace problematic global variable patterns. This guide explains the architecture, implementation patterns, and migration strategy.

---

## Architecture

### Design Principles

1. **Registry-Based Dispatch**: Subscribers register callbacks with the event system
2. **Type-Safe Events**: Enum-based event types prevent string-based lookup
3. **Fail-Safe**: Silent behavior if no handlers registered (graceful degradation)
4. **Stateless**: No retained event state (handlers must store what they need)
5. **Synchronous**: Events dispatch immediately, handlers run in subscription order
6. **Fixed Capacity**: Max 4 subscribers per event type (embedded constraint)

### State Management

```
App Event System
  │
  ├─ Event Registry (8 event types, up to 4 each)
  │   ├─ APP_EVENT_SAVE_CHANNEL → [handler1, handler2, ...]
  │   ├─ APP_EVENT_SAVE_VFO → [handler3, ...]
  │   └─ ...
  │
  └─ No retained event state (data passed as callback parameter)
```

---

## Usage Patterns

### Pattern 1: Publishing an Event (No Data)

**Before (Global Coupling)**:
```c
// In menu.c: Change UI state directly
gTxVfoIsActive = !gTxVfoIsActive;  // ❌ Implicit side effect
gUpdateStatus = true;
gUpdateDisplay = true;
```

**After (Event-Based)**:
```c
// In menu.c: Publish event only
APP_RaiseEvent(APP_EVENT_MODE_CHANGE, NULL);
```

### Pattern 2: Publishing an Event (With Data)

**Before**:
```c
// In menu.c: Modify global settings struct directly
gEeprom.TX_CHANNEL = selected_index;  // ❌ Tight coupling
```

**After**:
```c
// In menu.c: Raise event with payload
uint16_t payload = selected_index;
APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &payload);
```

### Pattern 3: Subscribing to Events

**Setup (Once at app startup)**:
```c
// In app.c: APP_Init() or similar
void APP_Init_EventHandlers(void) {
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, SETTINGS_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_VFO, RADIO_OnSaveVFO);
    // ... more subscriptions
}

int main(void) {
    // ... other init code
    APP_EventSystem_Init();
    APP_Init_EventHandlers();
    // ... rest of main
}
```

**Handler Function**:
```c
// In settings.c: Handler for SAVE_CHANNEL event
static void SETTINGS_OnSaveChannel(APP_EventType_t event, const void *data) {
    if (data == NULL) return;  // Defensive check
    
    uint16_t channel_idx = *(const uint16_t *)data;
    
    // Perform actual save operation
    SETTINGS_SaveChannel(channel_idx, &gCurrentChannel);
    
    // Update dependent UI state (event-based)
    APP_RaiseEvent(APP_EVENT_CHANNEL_SAVED, &channel_idx);
}
```

---

## Migration Strategy

### Phase 1: High-Impact Globals (Priority)

These cause the most cascading updates and bugs:

1. **`gEeprom.TxChannel` / `gTxVfoIsActive`**
   - Event: `APP_EVENT_SAVE_CHANNEL`
   - Subscribers: SETTINGS, RADIO, UI

2. **`gVfoInfo[gTxVfo].Frequency`**
   - Event: `APP_EVENT_FREQUENCY_CHANGE`
   - Subscribers: RADIO, UI, SCANNER

3. **`gEeprom.PowerLevel`**
   - Event: `APP_EVENT_POWER_CHANGE`
   - Subscribers: RADIO, UI

**Action Plan**:
- Replace direct assignments with `APP_RaiseEvent()`
- Add handler registrations in `APP_Init_EventHandlers()`
- Test with existing global fallback (run both systems in parallel initially)

### Phase 2: Medium-Impact Globals (Next)

3. **UART/USB command handlers**
   - Event: `APP_EVENT_UART_CMD_RX`
   - Data: Command packet pointer

4. **Scanner state**
   - Event: `APP_EVENT_SCANNER_FOUND`
   - Data: Channel index found

**Action Plan**:
- Same as Phase 1
- Add debug logging to verify event dispatch

### Phase 3: Fine-Grain Control (Optional)

Add events for:
- Individual menu changes
- Focus state changes  
- Animation/refresh timing

---

## Integration Checklist

### Per Event Type

- [ ] Define event type in `APP_EventType_t` enum
- [ ] Add descriptive comment (data format, usage)
- [ ] Identify all module dependencies
- [ ] Create handler function template
- [ ] Find all `APP_RaiseEvent()` call sites
- [ ] Replace global assignments with event raises
- [ ] Register handlers in `APP_Init_EventHandlers()`
- [ ] Test with logging enabled
- [ ] Document in ARCHITECTURE_EXTENSIONS.md

### Code Quality

- [ ] No event handlers modify unrelated globals
- [ ] Handlers are idempotent (safe to call twice)
- [ ] Data validation in handlers (NULL checks, bounds)
- [ ] Error paths do not modify application state
- [ ] No blocking operations in handlers (< 1ms)

---

## Testing Strategies

### Unit Testing Events

```c
// In tests/test_events.cpp
#include "App/app/events.h"

static int callback_invoked_count = 0;
static void test_handler(APP_EventType_t event, const void *data) {
    callback_invoked_count++;
}

void test_subscribe_and_raise() {
    APP_EventSystem_Init();
    ASSERT_EQ(true, APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, test_handler));
    
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, NULL);
    ASSERT_EQ(1, callback_invoked_count);
}

void test_unsubscribe() {
    APP_EventSystem_Init();
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, test_handler);
    APP_UnsubscribeEvent(APP_EVENT_SAVE_CHANNEL, test_handler);
    
    callback_invoked_count = 0;
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, NULL);
    ASSERT_EQ(0, callback_invoked_count);
}
```

### Integration Testing

```c
// Test that UI change triggers settings save
void test_ui_change_triggers_settings_save() {
    bool settings_saved = false;
    
    // Handler that sets flag when save event fires
    void mock_settings_saver(APP_EventType_t e, const void *d) {
        settings_saved = true;
    }
    
    APP_EventSystem_Init();
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, mock_settings_saver);
    
    // Simulate user changing channel via UI
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, NULL);
    
    ASSERT_TRUE(settings_saved);
}
```

### Mock Pattern for Isolation

```c
// Test RADIO module independently of SETTINGS
static void RADIO_MockSettingsSaver(APP_EventType_t e, const void *d) {
    // No-op for testing RADIO behavior independently
}

void test_radio_frequency_update() {
    APP_EventSystem_Init();
    APP_SubscribeEvent(APP_EVENT_FREQUENCY_CHANGE, RADIO_MockSettingsSaver);
    
    // Test RADIO frequency logic without triggering SETTINGS
    uint32_t freq = 146500000;
    APP_RaiseEvent(APP_EVENT_FREQUENCY_CHANGE, &freq);
    
    ASSERT_EQ(146500000, gVfoInfo[0].Frequency);
}
```

---

## Common Pitfalls & Solutions

### Pitfall 1: Event Loop Chains

**Problem**: Handler A raises event B, which raises event C, creating implicit dependencies.

**Solution**:
```c
// ❌ DON'T: Create event loops
void handle_save_channel(APP_EventType_t e, const void *d) {
    SETTINGS_SaveChannel(...);
    APP_RaiseEvent(APP_EVENT_NOTIFY_UI, NULL);  // Loop back to UI
}

// ✅ DO: Keep events simple and acyclic
void handle_save_channel(APP_EventType_t e, const void *d) {
    SETTINGS_SaveChannel(...);
    // UI listens to SAVE_CHANNEL directly, no loop
}
```

### Pitfall 2: Data Lifetime Issues

**Problem**: Handler stores pointer to event data, data becomes invalid after callback.

**Solution**:
```c
// ❌ DON'T: Store pointer to callback data
static const uint8_t *g_saved_channel;
void handler(APP_EventType_t e, const void *d) {
    g_saved_channel = (const uint8_t *)d;  // Invalid after callback!
}

// ✅ DO: Copy data into persistent storage
static uint8_t g_saved_channel;
void handler(APP_EventType_t e, const void *d) {
    if (d) {
        g_saved_channel = *(const uint8_t *)d;  // Safe copy
    }
}
```

### Pitfall 3: Silent Failures

**Problem**: Event raised but no handlers subscribed → silent no-op.

**Solution**:
```c
// In development: Check subscription status
#ifdef DEBUG_EVENTS
void APP_RaiseEvent(APP_EventType_t event, const void *data) {
    if (g_subscriber_count[event] == 0) {
        UART_Printf("WARNING: Event %d raised with no subscribers\n", event);
    }
    // ... actual dispatch
}
#endif
```

---

## Related Files

- **Header**: [App/app/events.h](App/app/events.h)
- **Implementation**: [App/app/events.c](App/app/events.c)
- **Integration Points**:
  - [App/main.c](App/main.c) — Add `APP_EventSystem_Init()` to startup
  - [App/app/app.c](App/app/app.c) — Add `APP_Init_EventHandlers()`
  - [App/app/menu.c](App/app/menu.c) — Replace global assignments
  - [App/settings.c](App/settings.c) — Add event handlers

---

## Performance Considerations

- **Overhead**: Negligible (< 0.1ms per event on PY32F071)
- **Memory**: 512 bytes fixed (8 events × 4 ptrs × 4 bytes + 8 counters)
- **Latency**: Synchronous dispatch (handlers called immediately)
- **No Reentrancy**: Do not raise events from handlers

---

## Future Extensions

1. **Event Timestamps**: Track when events occurred
2. **Event Filtering**: Subscribe to subset of events (e.g., by module)
3. **Deferred Queue**: Queue events for later dispatch in main loop
4. **Event Logging**: Built-in debug trace buffer
5. **Payload Validation**: Automatic type-checking for data

---

## References

- Event-driven architecture: Evans, Domain-Driven Design (2003)
- Observer pattern: Gang of Four Design Patterns (1994)
- Embedded event systems: Quantum Leaps QP/C framework
