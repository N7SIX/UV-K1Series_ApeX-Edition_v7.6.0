<!--
=====================================================================================
Event System Quick Reference
Author: N7SIX, Sean
Version: v7.6.0 (ApeX Edition)
License: Apache License, Version 2.0
=====================================================================================
-->

# Event System Quick Reference

## TL;DR: Core API

```c
// Subscribe to an event
bool APP_SubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback);

// Unsubscribe from an event
bool APP_UnsubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback);

// Raise an event (call all handlers)
void APP_RaiseEvent(APP_EventType_t event, const void *data);

// Initialize system (call once at startup)
void APP_EventSystem_Init(void);
```

---

## Event Types

| Event | Purpose | Data Format | Subscribers |
|-------|---------|-------------|-------------|
| `APP_EVENT_SAVE_CHANNEL` | User selected TX channel | `uint8_t *channel_idx` | SETTINGS, RADIO, UI |
| `APP_EVENT_SAVE_VFO` | VFO state changed | `VFO_Info_t *vfo` | SETTINGS, RADIO |
| `APP_EVENT_FREQUENCY_CHANGE` | Frequency updated | `uint32_t *frequency` | RADIO, UI |
| `APP_EVENT_MODE_CHANGE` | Operating mode changed | `uint8_t *mode` | RADIO, UI |
| `APP_EVENT_POWER_CHANGE` | TX power changed | `uint8_t *power_level` | RADIO, UI |
| `APP_EVENT_UART_CMD_RX` | UART command received | `uint8_t *cmd_buffer` | UART, APP |
| `APP_EVENT_SCANNER_FOUND` | Scanner found frequency | `uint32_t *frequency` | SCANNER, UI |

---

## Quick Start (3 Steps)

### 1. Emit Event (in source module)
```c
// menu.c: User selected something
uint8_t channel = 5;
APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &channel);
```

### 2. Create Handler (in target module)
```c
// settings.c: Handle the event
static void OnSaveChannel(APP_EventType_t event, const void *data) {
    uint8_t ch = *(uint8_t *)data;
    SETTINGS_SaveChannel(ch, ...);
}
```

### 3. Register Handler (in app.c startup)
```c
// app.c: APP_Init_EventHandlers()
APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, OnSaveChannel);
```

---

## Common Handler Patterns

### Simple Handler
```c
static void MyHandler(APP_EventType_t event, const void *data) {
    if (data == NULL) return;
    
    MyDataType *d = (MyDataType *)data;
    // Do work...
}

APP_SubscribeEvent(APP_EVENT_SOMETHING, MyHandler);
```

### Conditional Handler (Validate Data)
```c
static void ValidatingHandler(APP_EventType_t event, const void *data) {
    if (data == NULL) return;
    
    uint32_t value = *(uint32_t *)data;
    if (value < MIN_VALUE || value > MAX_VALUE) {
        return;  // Ignore invalid data
    }
    
    // Do work...
}
```

### Handler with Logging
```c
static void LoggingHandler(APP_EventType_t event, const void *data) {
    UART_Printf("Event %d fired\n", event);
    
    if (data) {
        UART_Printf("  Data: %p\n", data);
    }
    
    // Do work...
}
```

---

## Do's and Don'ts

### ✅ DO

- Subscribe in `APP_Init_EventHandlers()` (once)
- Copy event data into handler-owned storage
- Keep handlers short (< 1ms)
- Validate data (NULL check, bounds check)
- Raise subsequent events if needed
- Document expected data format

### ❌ DON'T

- Store pointers to event data
- Raise events from event handlers (creates loops)
- Block in handlers
- Modify unrelated global state
- Assume continuous subscription
- Raise events from ISRs

---

## Debugging

### Check Subscriptions
```c
// List current subscribers (add your own debug function)
void DEBUG_DumpEventSubscribers(void) {
    for (int i = 0; i < _APP_EVENT_MAX; i++) {
        UART_Printf("Event %d: %d subscribers\n", i, g_subscriber_count[i]);
    }
}
```

### Add Trace Logging
```c
static void DebugTracer(APP_EventType_t event, const void *data) {
    UART_Printf("[EVENT %d] fired with data=%p\n", event, data);
}

// Subscribe tracer to all events
for (int i = 0; i < _APP_EVENT_MAX; i++) {
    APP_SubscribeEvent((APP_EventType_t)i, DebugTracer);
}
```

### Verify Handler Called
```c
static int handler_call_count = 0;

static void CountingHandler(APP_EventType_t event, const void *data) {
    handler_call_count++;
}

// Test
APP_EventSystem_Init();
APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, CountingHandler);
APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, NULL);
assert(handler_call_count == 1);  // Should be true
```

---

## Limitations

- **Max 4 subscribers per event** (to save memory)
- **No event priority** (handlers called in subscription order)
- **Synchronous only** (no queue/deferred dispatch)
- **No reentrancy** (don't raise events from handlers)
- **No type-safety for data** (cast responsibility on handler)

---

## Files

| File | Purpose |
|------|---------|
| `App/app/events.h` | Header with API and event types |
| `App/app/events.c` | Implementation |
| `Documentation/EVENT_SYSTEM_GUIDE.md` | Full guide with patterns |
| `Documentation/EVENT_SYSTEM_EXAMPLES.md` | Real-world examples |

---

## Migration Path

1. **Phase 1**: TX channel change (`APP_EVENT_SAVE_CHANNEL`)
2. **Phase 2**: Frequency change (`APP_EVENT_FREQUENCY_CHANGE`)
3. **Phase 3**: Power changes (`APP_EVENT_POWER_CHANGE`)
4. **Phase 4**: Mode changes (`APP_EVENT_MODE_CHANGE`)

Each phase:
1. Add event type to enum
2. Find all emitters (global assignments)
3. Replace with `APP_RaiseEvent()`
4. Create handlers in dependent modules
5. Test with debug logging

---

## Integration Checklist

- [ ] Add `APP_EventSystem_Init()` call in `App/main.c:main()`
- [ ] Create `APP_Init_EventHandlers()` in `App/app/app.c`
- [ ] Include `events.h` in modules that use events
- [ ] Create first event handler (TX channel)
- [ ] Test event dispatch with debug logging
- [ ] Add unit tests for event system
- [ ] Document custom events in ARCHITECTURE_EXTENSIONS.md

---

## Example: Full Integration

**Step 1: app.h**
```c
void APP_Init_EventHandlers(void);
```

**Step 2: app.c**
```c
void APP_Init_EventHandlers(void) {
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, SETTINGS_OnSaveChannel);
    APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, RADIO_OnSaveChannel);
    // ... more subscriptions
}
```

**Step 3: main.c**
```c
int main(void) {
    BOARD_Init();
    // ... other init
    APP_EventSystem_Init();
    APP_Init_EventHandlers();
    
    while (1) {
        APP_Update();
    }
}
```

**Step 4: menu.c**
```c
void SelectChannel(uint8_t ch) {
    APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &ch);
    // No more direct global writes!
}
```

**Step 5: settings.c**
```c
static void SETTINGS_OnSaveChannel(APP_EventType_t e, const void *d) {
    uint8_t ch = *(uint8_t *)d;
    gEeprom.TX_CHANNEL = ch;
    SETTINGS_SaveToEeprom();
}
```

---

**See Also**: [EVENT_SYSTEM_GUIDE.md](EVENT_SYSTEM_GUIDE.md), [EVENT_SYSTEM_EXAMPLES.md](EVENT_SYSTEM_EXAMPLES.md)
