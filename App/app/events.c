/**
 * =====================================================================================
 * @file        events.c
 * @brief       Application event system implementation
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @author      Sean (Event System Architecture, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Professional event-driven architecture for embedded radio firmware."
 * =====================================================================================
 *
 * Registry-based event dispatch with subscriber callbacks.
 * Designed for low memory footprint and deterministic behavior.
 */

#include "events.h"
#include <string.h>

// =============================================================================
// CONFIG
// =============================================================================

/** Maximum number of subscribers per event type */
#define MAX_SUBSCRIBERS_PER_EVENT 4

// =============================================================================
// STATE
// =============================================================================

/**
 * Subscriber registry for each event type.
 * Each event can have up to MAX_SUBSCRIBERS_PER_EVENT callbacks.
 */
static APP_EventCallback_t g_subscribers[_APP_EVENT_MAX][MAX_SUBSCRIBERS_PER_EVENT];

/**
 * Number of active subscribers for each event type.
 */
static uint8_t g_subscriber_count[_APP_EVENT_MAX];

// =============================================================================
// IMPLEMENTATION
// =============================================================================

bool APP_SubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback)
{
    // Validate inputs
    if (event >= _APP_EVENT_MAX || callback == NULL) {
        return false;
    }

    // Check if we've hit the subscription limit
    if (g_subscriber_count[event] >= MAX_SUBSCRIBERS_PER_EVENT) {
        return false;
    }

    // Check for duplicate subscriptions (optional safety measure)
    for (int i = 0; i < g_subscriber_count[event]; i++) {
        if (g_subscribers[event][i] == callback) {
            // Already subscribed - could return false for strict prevention
            // or allow it to subscribe again. We'll allow duplicates for
            // flexibility, but callers should avoid this.
            break;
        }
    }

    // Add subscriber
    g_subscribers[event][g_subscriber_count[event]] = callback;
    g_subscriber_count[event]++;

    return true;
}

bool APP_UnsubscribeEvent(APP_EventType_t event, APP_EventCallback_t callback)
{
    // Validate input
    if (event >= _APP_EVENT_MAX || callback == NULL) {
        return false;
    }

    // Search for the callback
    for (int i = 0; i < g_subscriber_count[event]; i++) {
        if (g_subscribers[event][i] == callback) {
            // Found it - remove by shifting remaining subscribers down
            for (int j = i; j < g_subscriber_count[event] - 1; j++) {
                g_subscribers[event][j] = g_subscribers[event][j + 1];
            }
            g_subscriber_count[event]--;
            return true;
        }
    }

    // Not found
    return false;
}

void APP_RaiseEvent(APP_EventType_t event, const void *data)
{
    // Validate event type
    if (event >= _APP_EVENT_MAX) {
        return;
    }

    // Call all registered subscribers for this event
    for (int i = 0; i < g_subscriber_count[event]; i++) {
        APP_EventCallback_t callback = g_subscribers[event][i];
        if (callback != NULL) {
            callback(event, data);
        }
    }
}

void APP_EventSystem_Init(void)
{
    // Clear all subscriber registries
    memset(g_subscribers, 0, sizeof(g_subscribers));
    memset(g_subscriber_count, 0, sizeof(g_subscriber_count));

    // Future: Add logging/statistics tracking here
}
