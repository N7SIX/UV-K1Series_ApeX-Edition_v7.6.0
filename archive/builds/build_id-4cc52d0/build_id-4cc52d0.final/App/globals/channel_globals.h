/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef GLOBALS_CHANNEL_H
#define GLOBALS_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define FM_CHANNELS_MAX 48
#define MR_CHANNELS_MAX 1024
#define MR_CHANNELS_LIST 24
// CACHE-BASED OPTIMIZATION: Only keep active channels in RAM
// Full array stays in EEPROM, cache holds ~10 most-used channels
#define MR_CHANNELS_CACHE_SIZE 10

#define IS_MR_CHANNEL(x)       ((x) >= MR_CHANNEL_FIRST && (x) <= MR_CHANNEL_LAST)
#define IS_FREQ_CHANNEL(x)     ((x) >= FREQ_CHANNEL_FIRST && (x) <= FREQ_CHANNEL_LAST)
#define IS_VALID_CHANNEL(x)    ((x) < LAST_CHANNEL)
#define IS_NOAA_CHANNEL(x)     ((x) >= NOAA_CHANNEL_FIRST && (x) <= NOAA_CHANNEL_LAST)

enum {
    MR_CHANNEL_FIRST   = 0,
    MR_CHANNEL_LAST    = MR_CHANNELS_MAX - 1,
    FREQ_CHANNEL_FIRST = MR_CHANNELS_MAX,
    FREQ_CHANNEL_LAST  = MR_CHANNELS_MAX + 6,
    NOAA_CHANNEL_FIRST = MR_CHANNELS_MAX + 7,
    NOAA_CHANNEL_LAST  = MR_CHANNELS_MAX + 16,
    LAST_CHANNEL
};

typedef union {
    struct {
        uint16_t
            band :      3,
            compander : 2,
            unused_1 :  1,
            unused_2 :  1,
            exclude :   1,
            scanlist :  8;
    };
    uint16_t __val;
} ChannelAttributes_t;

// 
// Cache-Based Architecture
// 
//
// Instead of keeping all 1038 channel attributes in RAM (~ 2,000 bytes),
// we now keep only the active ones in a small cache (40 bytes).
//
// The full array remains in Flash and is loaded on-demand.
//
// SRAM Savings: ~ 2,000 bytes (84% reduction!)
// 

// Cache entry structure
typedef struct {
    uint16_t channel_id;                    // Which channel this is
    ChannelAttributes_t attributes;         // The actual attributes
    uint32_t access_time;                   // For LRU eviction (optional)
} MR_ChannelCache_t;

// The cache (small, stays in RAM)
extern MR_ChannelCache_t gMR_ChannelAttributes_Cache[MR_CHANNELS_CACHE_SIZE];

// The full channel attributes array now stays in Flash, not in RAM

// 
// Cache Access Functions (See misc.c for implementation)
// 

// Get channel attributes (from cache or Flash)
// Returns pointer to attributes, loads from Flash if not in cache

void MR_InitChannelAttributesCache(void);

ChannelAttributes_t* MR_GetChannelAttributes(uint16_t channel_id);

// Set channel attributes (updates both cache and Flash)
void MR_SetChannelAttributes(uint16_t channel_id, const ChannelAttributes_t* attributes);

// Invalidate cache (on Flash clear)
void MR_InvalidateChannelAttributesCache(void);

// Load channel attributes from Flash directly (internal use)
void MR_LoadChannelAttributesFromFlash(uint16_t channel_id, ChannelAttributes_t* attributes);

// Save channel attributes to Flash directly (internal use)
void MR_SaveChannelAttributesToFlash(uint16_t channel_id, const ChannelAttributes_t* attributes);

extern ChannelAttributes_t   gMR_ChannelAttributes_Current;  // Current VFO attributes (for speed)

extern uint16_t              gNextMrChannel;
extern uint8_t               gShowChPrefix;

#ifdef ENABLE_FEAT_N7SIX
    extern char gListName[MR_CHANNELS_LIST][4];
#endif

#endif