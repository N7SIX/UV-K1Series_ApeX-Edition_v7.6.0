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

// CW application states (NR7Y TX state machine)
typedef enum {
    CW_APP_INACTIVE = 0,
    CW_APP_TRANSMITTING,
    CW_APP_SUSPENDED
} CW_AppState_t;

// CW action codes returned by the keyer/playback engine
typedef enum {
    CW_ACTION_NONE = 0,
    CW_ACTION_CARRIER_ON,
    CW_ACTION_CARRIER_OFF,
    CW_ACTION_CARRIER_HOLD_ON
} CW_Action_t;

// CW element types for encoder
typedef enum {
    CW_ELEMENT_DIT = 0,
    CW_ELEMENT_DAH = 1,
    CW_ELEMENT_INTER_CHAR_SPACE = 2,
    CW_ELEMENT_INTER_WORD_SPACE = 3
} CW_ElementType_t;

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

// Display holdoff after TX state changes (10ms ticks)
#define CW_TX_DISPLAY_HOLDOFF_10MS  20

// Suspend timeout before ending TX (ms)
#define CW_SUSPEND_TIMEOUT_MS  200

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
extern CW_AppState_t gCW_AppState;
extern char          gCW_Message[CW_MSG_MAX_LEN + 1];
extern uint8_t       gCW_CursorPos;
extern uint8_t       gCW_WPM;
extern uint16_t      gCW_ToneFreq;
extern bool          gCW_PlaybackActive;
extern bool          gCW_PlaybackRepeat;
extern uint8_t       gCW_PlaybackMacroIndex;
extern uint16_t      gCW_MessageRepeatCountdown_500ms;
extern bool          gCW_PlayIndicatorOn;
extern uint16_t      gCW_SuspendCounter_1ms;
extern uint16_t      gCW_TxDisplayHoldoff_10ms;
extern bool          gCW_Recording;
extern bool          gPttIsPressed;

// Public API
void    CW_Init(void);
void    CW_Start(void);
void    CW_Stop(void);
void    CW_Toggle(void);
void    CW_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void    CW_TimeSlice10ms(void);
void    CW_Render(void);
void    CW_SendMessage(void);
void    CW_AppendChar(char c);
void    CW_DeleteChar(void);
bool    CW_IsActive(void);

// NR7Y TX state machine API
void    CW_AppInit(void);
void    CW_AppUpdate(void);
void    CW_EndTxNow(void);
void    CW_UpdateWPM(void);
void    CW_KeyerResetRuntime(void);
void    CW_KeyerReconfigure(bool enable);
bool    CW_CheckKeyerInputs(uint8_t new_mode);
void    CW_StartMacroPlayback(uint8_t macroIndex, bool repeat);
void    CW_StopPlayback(void);
CW_Action_t CW_PlaybackHandleState(void);
void    CW_PlaybackIndicatorDeadline(void);
void    CW_EncoderProcessElement(CW_ElementType_t element);
void    CW_AddToTxDisplay(char ch, bool hasSpace);
void    CW_ClearTxDisplay(void);
uint8_t CW_GetTxDisplayTail(char *display, uint8_t maxLen);
void    CW_StartRecording(uint8_t macroIndex);
void    CW_StopRecording(void);
uint8_t CW_LoadMacro(uint8_t macroIndex, char *buffer, uint8_t bufferSize);
void    CW_SaveMacro(uint8_t macroIndex, const char *buffer, uint8_t length);
uint8_t CW_GetMacroLength(uint8_t macroIndex);
bool    CW_ValidateChar(char ch);
uint8_t CW_FormatMacroDisplay(uint8_t macroIndex, char *display, uint8_t maxChars);
bool    CW_GetMorseForChar(char ch, uint8_t *pattern, uint8_t *length);

// Global systick counter (from scheduler.c)
extern volatile uint32_t gGlobalSysTickCounter;

// Keyer state flags
extern bool gCW_KeyerManagesPtt;
extern bool gCW_KeyerUsingSD1;

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

void CW_PlayDit(void);
void CW_PlayDah(void);
void CW_PlayCharacter(const char *morse);

// Entry point (called from main.c via F+7)
void APP_RunCW(void);

// Overlay: updates framebuffer for CW, called from UI_DisplayMain
void CW_Overlay(void);

// --- Keyer mode and input flags ---
#define CW_KEYER_MODE_IAMBIC_A    0
#define CW_KEYER_MODE_IAMBIC_B    1
#define CW_KEYER_MODE_ULTIMATIC   2
#define CW_KEYER_MODE_BUG         3
#define CW_KEYER_MODE_STRAIGHT    4

#define CW_KEY_FLAG_NO_KEYER      (1U << 0)
#define CW_KEY_FLAG_PORT_GROUND   (1U << 1)
#define CW_KEY_FLAG_PORT_RING     (1U << 2)
#define CW_KEY_FLAG_ADC           (1U << 3)
#define CW_KEY_FLAG_SIDE1         (1U << 4)
#define CW_KEY_FLAG_REVERSED      (1U << 5)

// CW input structure for hardware reads
typedef struct {
    bool dit;
    bool dah;
    bool dit_rise;
    bool dah_rise;
    bool last_is_dah;
} CW_Input;
