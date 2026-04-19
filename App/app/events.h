/**
 * =====================================================================================
 * @file        events.h
 * @brief       Application event subscription and dispatch system
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @author      Sean (Event System Architecture, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Professional event-driven architecture for embedded radio firmware."
 * =====================================================================================
 * 
 * Provides a callback-based event architecture to replace global variable coupling
 * patterns throughout the application. Events decouple UI modules (menu, scanner)
 * from core logic (app, settings, radio).
 *
 * ARCHITECTURE:
 * - Registry-based subscription system (max 4 callbacks per event type)
 * - Type-safe event raising with optional data payload
 * - Silent fail-safe behavior if no subscribers registered
 * - Professional embedded systems pattern (similar to embedded event buses)
 *
 * USAGE EXAMPLE:
 *   // In menu.c - publish event
 *   APP_RaiseEvent(APP_EVENT_SAVE_CHANNEL, &channel_data);
 *   
 *   // In app.c - subscribe to event
 *   void HandleSaveChannel(APP_EventType_t event, void *data) {
 *       uint8_t *ch_idx = (uint8_t*)data;
 *       SETTINGS_SaveChannel(*ch_idx, ...);
 *   }
 *   APP_SubscribeEvent(APP_EVENT_SAVE_CHANNEL, HandleSaveChannel);
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// EVENT TYPE DEFINITIONS
// =============================================================================

/**
 * @enum APP_EventType_t
 * @brief Application-wide event types
 *
 * All significant actions that cross module boundaries should raise events.
 * This prevents tight coupling and enables better testing/monitoring.
 */
typedef enum {
    APP_EVENT_SAVE_CHANNEL,              /**< Save current TX channel configuration */
    APP_EVENT_SAVE_VFO,                  /**< Save VFO state changes */
    APP_EVENT_FREQUENCY_CHANGE,          /**< User changed center frequency */
    APP_EVENT_MODE_CHANGE,               /**< Operating mode changed (RX/TX/etc) */
    APP_EVENT_POWER_CHANGE,              /**< TX power or battery state change */
    APP_EVENT_UART_CMD_RX,               /**< UART command received */
    APP_EVENT_SCANNER_FOUND,             /**< Scanner found a channel */
    
    // Add more events as needed - keep in priority order
    _APP_EVENT_MAX                       /**< Sentinel - do not use */
} APP_EventType_t;

// =============================================================================
// CALLBACK INTERFACE
// =============================================================================

/**
 * @typedef APP_EventCallback_t
 * @brief Callback function signature for event handlers
 *
 * @param event The event type that was raised
 * @param data  Optional pointer to event-specific data (caller-allocated)
 *
 * IMPORTANT: data is only valid during callback execution. Handlers must
 * copy any data they need to persist beyond the callback.
 */
typedef void (*APP_EventCallback_t)(APP_EventType_t event, const void *data);

// =============================================================================
// EVENT SYSTEM INTERFACE
// =============================================================================

/**
 * @brief Subscribe a handler to an event type
 *
 * Registers a callback to be invoked whenever the specified event is raised.
 * Multiple handlers can subscribe to the same event (max 4 per event).
 *
 * @param event    Event type to subscribe to
 * @param callback Handler function pointer (must not be NULL)
 * @return true if successfully subscribed, false if subscription limit exceeded
 *
 * NOTE: Safe to call multiple times with same callback (idempotent-safe,
 * though duplicates will cause handler to be called multiple times).
 */
bool APP_SubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback);

/**
 * @brief Unsubscribe a handler from an event type
 *
 * Removes a callback from the subscriber list for the specified event.
 *
 * @param event    Event type to unsubscribe from
 * @param callback Handler function pointer to remove
 * @return true if successfully unsubscribed, false if callback not found
 */
bool APP_UnsubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback);

/**
 * @brief Raise an event and invoke all registered handlers
 *
 * Synchronously calls all handlers subscribed to this event type with
 * the provided data (if any). Handlers are called in subscription order.
 * Exceptions would propagate normally if they occur.
 *
 * @param event Event type to raise
 * @param data  Optional event payload (NULL if no data needed)
 *
 * THREAD SAFETY: This function is NOT reentrant - do not raise events
 * from within event handlers. If you need this, add a deferred event queue.
 *
 * SILENT BEHAVIOR: If no handlers are subscribed, this silently succeeds.
 * This is intentional to allow graceful degradation.
 */
void APP_RaiseEvent(APP_EventType_t event, const void *data);

/**
 * @brief Initialize the event system
 *
 * Should be called once at application startup, before any events are raised.
 * Currently a no-op but provided for future extensibility (logging, stats, etc).
 */
void APP_EventSystem_Init(void);

#endif // APP_EVENTS_H
