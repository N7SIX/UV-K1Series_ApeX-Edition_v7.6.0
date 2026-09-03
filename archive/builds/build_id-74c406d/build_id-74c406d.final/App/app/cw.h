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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/keyboard.h"

// CW state machine (ApeX UI states)
typedef enum {
    CW_IDLE = 0,
    CW_COMPOSING,
    CW_SENDING
} CW_State_t;

// CW message maximum length
#define CW_MSG_MAX_LEN   80

// Default WPM (Words Per Minute)
// A "dit" duration = 1200 / WPM milliseconds
#define CW_DEFAULT_WPM   20

// Dit duration in ms at default WPM: 1200 / 20 = 60ms
// Dah = 3 * dit, inter-element = 1 * dit, inter-char = 3 * dit, inter-word = 7 * dit

// Default CW tone frequency (Hz)
// 800 Hz can improve audibility on some handheld speaker/audio paths.
#define CW_TONE_FREQ     800

// Default RSSI threshold for CW RX detection (dBm)
// Set slightly above typical noise floor to avoid false triggers from
// ambient noise, but still sensitive enough for real CW signals.
#define CW_RX_RSSI_THRESHOLD_DEFAULT  (-105)

// Key press tick reset timeout (ms). Used to clear multi-tap key state.
#define CW_KEY_TICK_RESET_MS   800

// PTT long-press threshold for typed-message TX (10ms ticks; 40 = 400ms).
// Aligned with key_repeat_delay_10ms (400ms) used by the main keypad so the CW
// short/long PTT threshold feels identical to the rest of the UI.
#define CW_PTT_LONG_PRESS_10MS  40

// LCD layout - CW uses lines 24-55 (pages 3-6)
// gFrameBuffer[3-4] for TX text (wrapping), gFrameBuffer[5] for RX decode, gFrameBuffer[6] for status
#define CW_LINE_TX1      3   // gFrameBuffer[3] - TX text row 1 (lines 24-31)
#define CW_LINE_TX2      4   // gFrameBuffer[4] - TX text row 2 (lines 32-39)
#define CW_LINE_DECODE   5   // gFrameBuffer[5] - RX Morse + decoded text (lines 40-47)
#define CW_LINE_STATUS   6   // gFrameBuffer[6] - status + gauge (lines 48-55)
#define CW_CHARS_PER_TX_LINE 17

// CW symbol definitions for TX progress visualization
#define CW_GAUGE_WIDTH   113 // pixels for the gauge bar

extern CW_State_t    gCW_State;
extern char          gCW_Message[CW_MSG_MAX_LEN + 1];
extern uint8_t       gCW_CursorPos;
extern uint8_t       gCW_WPM;
extern uint16_t      gCW_ToneFreq;
extern bool          gPttIsPressed;

// Public API
void    CW_Init(void);
void    CW_Start(void);
void    CW_Stop(void);
void    CW_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void    CW_TimeSlice10ms(void);
void    CW_Render(void);
void    CW_SendMessage(void);
void    CW_AppendChar(char c);
void    CW_DeleteChar(void);
bool    CW_IsActive(void);

// Global systick counter (from scheduler.c)
extern volatile uint32_t gGlobalSysTickCounter;

// Morse lookup
const char * CW_CharToMorse(char c);

// CW tone timing - shared between TX and decoder
extern uint16_t gCW_DitMs;
extern uint16_t gCW_DahMs;
extern uint16_t gCW_InterElemMs;
extern uint16_t gCW_InterCharMs;
extern uint16_t gCW_InterWordMs;

// Shared Morse character map (defined in cw.c, used by cwdecoder.c)
typedef struct {
    char ch;
    const char *morse;
} CW_CharMap_t;

extern const CW_CharMap_t CW_CHAR_MAP[];
extern const uint8_t      CW_CHAR_MAP_COUNT;

// Entry point (called from main.c via F+7)
void APP_RunCW(void);

// Overlay: updates framebuffer for CW, called from UI_DisplayMain
void CW_Overlay(void);
