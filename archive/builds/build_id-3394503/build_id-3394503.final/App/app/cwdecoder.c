/* Copyright 2026 Sean, N7SIX
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/cw.h"
#include "app/cwdecoder.h"

#include <string.h>

#include "driver/st7565.h"
#include "ui/ui.h"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define CW_RX_MORSE_MAX_LEN 10
#define CW_SIGNAL_THRESHOLD  32
#define CW_SIGNAL_FULL       64
#define CW_NOISE_FLOOR       20
#define CW_TRACE_BUF_SIZE 128

// ---------------------------------------------------------------------------
// Classic CW decoder – simple and robust
//
// States:  IDLE → (tone on) → MARK → (tone off) → GAP → (decode) → IDLE
//
// Timing (from WPM setting):
//   ditTicks      = gCW_DitMs / 10   (1 tick = 10ms)
//   dah threshold = 2 * ditTicks
//   char gap      = 3 * ditTicks
//   word gap      = 7 * ditTicks
//
// - Mark < 2*ditTicks  → dit
// - Mark >= 2*ditTicks → dah
// - Gap < 3*ditTicks   → inter-element (continue same char)
// - Gap >= 3*ditTicks  → decode character
// - Gap >= 7*ditTicks  → decode character + add word space
// ---------------------------------------------------------------------------

// CW_CHAR_MAP is shared from cw.h (defined in cw.c)

static char     gCW_DecodeText[CW_MSG_MAX_LEN + 1] = {0};
static uint8_t  gCW_DecodeCursor = 0;
static char     gCW_RxMorse[CW_RX_MORSE_MAX_LEN + 1] = {0};
static uint8_t  gCW_RxMorseLen = 0;
static uint16_t gCW_RxMarkTicks = 0;
static uint16_t gCW_RxSpaceTicks = 0;
static bool     gCW_RxSignalPrev = false;
static bool     gCW_RxActive = false;
static uint8_t  gCW_RxActiveDebounce = 0;
static uint8_t  gCW_RxInactiveDebounce = 0;
static uint16_t gCW_RxTraceClock = 0;
static bool     gCW_RxToneState = false;
static uint8_t  gCW_RxToneOnDebounce = 0;
static uint8_t  gCW_RxToneOffDebounce = 0;
static bool     gCW_StartupDelay = false;
static uint8_t  gCW_TraceHistory[CW_TRACE_BUF_SIZE];
static uint8_t  gCW_TracePeak[CW_TRACE_BUF_SIZE];
static uint8_t  gCW_TraceHead = 0;
static int16_t  gCW_RxCurrentRssi = -120;  // Last RSSI sample for confidence calc
static uint16_t gCW_RxSignalSum = 0;       // Sum of signal levels in current char
static uint8_t  gCW_RxElementsInChar = 0;  // Element count for current char
static uint8_t  gCW_RxCharConfidence = 100; // Computed confidence for last char
static int16_t  gCW_RxNoiseFloor = -120;   // Adaptive noise floor (capped at -97)

typedef enum {
    CW_RX_STATE_IDLE = 0,
    CW_RX_STATE_MARK,
    CW_RX_STATE_GAP
} CW_RxDecoderState_t;

static CW_RxDecoderState_t gCW_RxDecoderState = CW_RX_STATE_IDLE;

// ---------------------------------------------------------------------------
// Morse tree (binary tree, depth-first, left=dot, right=dash)
// Covers A-Z, 0-9. Punctuation uses fallback array scan.
// ---------------------------------------------------------------------------
static const char MORSE_TREE[64] = {
    '*',  //  0: unused
    '*',  //  1: root
    'E', 'T',  //  2-3
    'I', 'A', 'N', 'M',  //  4-7
    'S', 'U', 'R', 'W', 'D', 'K', 'G', 'O',  //  8-15
    'H', 'V', 'F', '*', 'L', '*', 'P', 'J', 'B', 'X', 'C', 'Y', 'Z', 'Q', '*', '*',  // 16-31
    '5', '4', '*', '3', '*', '*', '*', '2', '*', '*', '+', '*', '*', '*', '*', '1',  // 32-47
    '6', '=', '/', '*', '*', '*', '*', '*', '7', '*', '*', '*', '8', '*', '9', '0'   // 48-63
};

// ---------------------------------------------------------------------------
// Decode helpers
// ---------------------------------------------------------------------------
static char CW_DecodeMorseTree(const char *morse)
{
    int index = 1;  // Start at root

    for (uint8_t i = 0; morse[i] != '\0'; i++)
    {
        if (morse[i] == '.')
            index = 2 * index;
        else if (morse[i] == '-')
            index = (2 * index) + 1;
        else
            return '?';

        if (index >= 64)
            return '?';
    }

    char decoded = MORSE_TREE[index];
    return (decoded == '*') ? '?' : decoded;
}

static const char *CW_MorseToDecodedToken(const char *morse)
{
    static char oneChar[2];

    if (morse == NULL || morse[0] == '\0')
        return " ";

    // Fast path: try tree lookup first (covers A-Z, 0-9)
    char decoded = CW_DecodeMorseTree(morse);
    if (decoded != '?')
    {
        oneChar[0] = decoded;
        oneChar[1] = '\0';
        return oneChar;
    }

    // Fallback: scan full character map for punctuation
    for (uint8_t i = 0; i < CW_CHAR_MAP_COUNT; i++)
    {
        if (strcmp(morse, CW_CHAR_MAP[i].morse) == 0)
        {
            oneChar[0] = CW_CHAR_MAP[i].ch;
            oneChar[1] = '\0';
            return oneChar;
        }
    }

    return "?";
}

// ---------------------------------------------------------------------------
// Output buffer
// ---------------------------------------------------------------------------
static void CW_AppendDecodedText(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return;

    for (uint8_t i = 0; text[i] != '\0'; i++)
    {
        if (gCW_DecodeCursor >= CW_MSG_MAX_LEN)
        {
            memmove(gCW_DecodeText, gCW_DecodeText + 1, CW_MSG_MAX_LEN - 1);
            gCW_DecodeCursor = CW_MSG_MAX_LEN - 1;
        }

        gCW_DecodeText[gCW_DecodeCursor++] = text[i];
        gCW_DecodeText[gCW_DecodeCursor] = '\0';
    }
}

// ---------------------------------------------------------------------------
// Trace graph buffer
// ---------------------------------------------------------------------------
static void CW_PushTraceSample(uint8_t level)
{
    gCW_TraceHistory[gCW_TraceHead] = level;
    if (level > gCW_TracePeak[gCW_TraceHead])
        gCW_TracePeak[gCW_TraceHead] = level;
    else if (gCW_TracePeak[gCW_TraceHead] > 0)
        gCW_TracePeak[gCW_TraceHead]--;

    gCW_TraceHead = (gCW_TraceHead + 1) & 0x7F;
}

// ---------------------------------------------------------------------------
// Core decoding logic
// ---------------------------------------------------------------------------

// Classify a completed mark as dit or dah and append to Morse buffer.
// Threshold: >= 2*ditTicks → dah, else dit.
static void CW_ClassifyMark(uint16_t markTicks, uint16_t ditTicks)
{
    if (markTicks < 2)
        return;  // ignore sub-tick noise

    if (gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
    {
        if (markTicks >= (ditTicks * 2U))
            gCW_RxMorse[gCW_RxMorseLen++] = '-';
        else
            gCW_RxMorse[gCW_RxMorseLen++] = '.';
        gCW_RxMorse[gCW_RxMorseLen] = '\0';
        CW_Render();  // show live Morse pattern
    }

    // Accumulate signal strength for confidence calculation.
    // gCW_RxCurrentRssi holds the last RSSI sample during this mark.
    // Convert to positive offset from noise floor for averaging.
    int16_t signalOffset = gCW_RxCurrentRssi - gCW_RxNoiseFloor;
    if (signalOffset < 0) signalOffset = 0;
    if (signalOffset > 40) signalOffset = 40;  // cap at 40 dB above noise
    gCW_RxSignalSum += (uint16_t)signalOffset;
    gCW_RxElementsInChar++;
}

// Decode the accumulated Morse pattern and append the character.
static void CW_DecodeCharacter(bool addWordSpace)
{
    if (gCW_RxMorseLen == 0)
        return;

    // Compute confidence from accumulated signal strength.
    // Average signal offset per element, map 0-40 dB to 0-100%.
    // Adaptive threshold: short chars (1-2 elements) need less SNR.
    if (gCW_RxElementsInChar > 0)
    {
        uint16_t avgSignal = gCW_RxSignalSum / gCW_RxElementsInChar;
        // Map: 0 dB above noise = 0%, 20 dB above noise = 100%
        // Short chars (1-2 elements) get a floor of 20%
        uint8_t minConfidence = (gCW_RxElementsInChar <= 2) ? 20 : 0;
        uint8_t rawConfidence = (uint8_t)((avgSignal * 100U) / 20U);
        if (rawConfidence > 100) rawConfidence = 100;
        gCW_RxCharConfidence = (rawConfidence < minConfidence) ? minConfidence : rawConfidence;
    }
    else
    {
        gCW_RxCharConfidence = 100;
    }

    // Reset accumulators for next character
    gCW_RxSignalSum = 0;
    gCW_RxElementsInChar = 0;

    gCW_RxMorse[gCW_RxMorseLen] = '\0';
    const char *decodedToken = CW_MorseToDecodedToken(gCW_RxMorse);
    if (decodedToken != NULL && decodedToken[0] != '\0')
    {
        CW_AppendDecodedText(decodedToken);
    }

    gCW_RxMorseLen = 0;
    gCW_RxMorse[0] = '\0';

    if (addWordSpace)
    {
        CW_AppendDecodedText(" ");
    }

    CW_Render();  // update display with decoded text
}

// ---------------------------------------------------------------------------
// Tone detection (simple fixed threshold with 1-tick debounce)
// ---------------------------------------------------------------------------
static void CW_DetectTone(int16_t rssi_dBm)
{
    // Adaptive noise floor tracking: track minimum RSSI during silence,
    // capped at -97 dBm to prevent AGC-hold drift.
    if (!gCW_RxToneState && !gCW_RxSignalPrev)
    {
        if (rssi_dBm < gCW_RxNoiseFloor)
            gCW_RxNoiseFloor = rssi_dBm;
        // Slow upward recovery to track changing band conditions
        else if (gCW_RxNoiseFloor < -97)
            gCW_RxNoiseFloor++;
    }
    // Hard cap: noise floor never exceeds -97 dBm
    if (gCW_RxNoiseFloor > -97)
        gCW_RxNoiseFloor = -97;

    // Signal detection: RSSI must be above the noise floor by margin OR
    // above the absolute threshold (whichever is higher sensitivity).
    const int16_t effectiveThreshold = (gCW_RxNoiseFloor + 10 > CW_RX_RSSI_THRESHOLD_DEFAULT)
        ? (gCW_RxNoiseFloor + 10)
        : CW_RX_RSSI_THRESHOLD_DEFAULT;
    const bool signalNow = (rssi_dBm >= effectiveThreshold);

    if (signalNow)
    {
        // Every RSSI tick logged for confidence calculation
        gCW_RxCurrentRssi = rssi_dBm;
        if (gCW_RxToneOnDebounce < 0xFF) gCW_RxToneOnDebounce++;
        gCW_RxToneOffDebounce = 0;
        if (!gCW_RxToneState && gCW_RxToneOnDebounce >= 1)
        {
            gCW_RxToneState = true;
        }
    }
    else
    {
        if (gCW_RxToneOffDebounce < 0xFF) gCW_RxToneOffDebounce++;
        gCW_RxToneOnDebounce = 0;
        if (gCW_RxToneState && gCW_RxToneOffDebounce >= 1)
        {
            gCW_RxToneState = false;
        }
    }
}

// ---------------------------------------------------------------------------
// RX activation: only show trace graph when we're actually receiving
// ---------------------------------------------------------------------------
static void CW_HandleRxActivation(bool signalNow)
{
    if (signalNow && !gCW_RxActive)
    {
        if (gCW_RxActiveDebounce < 0xFF) gCW_RxActiveDebounce++;
        if (gCW_RxActiveDebounce >= CW_RX_ACTIVATE_TICKS)
        {
            gCW_RxActive = true;
            gCW_RxActiveDebounce = 0;
            CW_Render();  // update status line: MON -> RX
        }
    }
    else if (!signalNow && gCW_RxActive)
    {
        if (gCW_RxInactiveDebounce < 0xFF) gCW_RxInactiveDebounce++;
        if (gCW_RxInactiveDebounce >= CW_RX_DEACTIVATE_TICKS)
        {
            gCW_RxActive = false;
            gCW_RxActiveDebounce = 0;
            gCW_RxInactiveDebounce = 0;
            CW_Render();  // update status line: RX -> MON
        }
    }
    else if (!signalNow && !gCW_RxActive)
    {
        gCW_RxActiveDebounce = 0;
        gCW_RxInactiveDebounce = 0;
    }
}

// ---------------------------------------------------------------------------
// Trace graph update
// ---------------------------------------------------------------------------
static void CW_UpdateTraceBuffer(bool signalNow, int16_t rssi_dBm)
{
    if (!gCW_RxActive)
    {
        // Idle: push 0 at half rate to show flat line
        if (gCW_RxTraceClock == 0) CW_PushTraceSample(0);
        gCW_RxTraceClock = (gCW_RxTraceClock + 1) & 1u;
    }
    else
    {
        // Active: advance trace at 1 sample per tick during tone,
        // slower during gaps to stretch the waveform visually
        const uint16_t ditTicks = (uint16_t)MAX(1, (gCW_DitMs + 5) / 10);
        const uint16_t traceAdvanceTicks = signalNow ? 1U : MAX(2U, (ditTicks + 1U) / 2U);

        if (gCW_RxTraceClock >= traceAdvanceTicks)
        {
            gCW_RxTraceClock = 0;
            uint8_t sigLevel = (uint8_t)(rssi_dBm + 120);
            if (sigLevel > CW_SIGNAL_FULL) sigLevel = CW_SIGNAL_FULL;
            if (signalNow) sigLevel = CW_SIGNAL_FULL;
            else if (sigLevel > CW_NOISE_FLOOR) sigLevel = CW_NOISE_FLOOR;
            CW_PushTraceSample(sigLevel);
        }
        else
        {
            gCW_RxTraceClock++;
        }
    }
}

// ---------------------------------------------------------------------------
// Public: draw the trace graph on CW_LINE_DECODE (gFrameBuffer[5])
// ---------------------------------------------------------------------------
void CW_Decoder_DrawSignalGraph(void)
{
    memset(gFrameBuffer[CW_LINE_DECODE], 0, LCD_WIDTH);

    #define Y_TO_BIT(y)  ((uint8_t)(1u << (6u - (y))))
    #define LEVEL_TO_Y(l)  ((uint8_t)(((l) >= 64u) ? 0u : (6u - ((l) * 7u) / 65u)))

    for (uint8_t col = 0; col < 127; col++)
    {
        const uint8_t idx0 = (uint8_t)((gCW_TraceHead + col) & 0x7Fu);
        const uint8_t idx1 = (uint8_t)((gCW_TraceHead + col + 1) & 0x7Fu);
        const uint8_t level0 = (gCW_TraceHistory[idx0] > gCW_TracePeak[idx0]) ? gCW_TraceHistory[idx0] : gCW_TracePeak[idx0];
        const uint8_t level1 = (gCW_TraceHistory[idx1] > gCW_TracePeak[idx1]) ? gCW_TraceHistory[idx1] : gCW_TracePeak[idx1];

        uint8_t y0 = LEVEL_TO_Y(level0);
        uint8_t y1 = LEVEL_TO_Y(level1);

        gFrameBuffer[CW_LINE_DECODE][col] |= Y_TO_BIT(y0);
        if (y1 > y0)
        {
            for (uint8_t y = y0; y <= y1; y++)
                gFrameBuffer[CW_LINE_DECODE][col] |= Y_TO_BIT(y);
        }
        else if (y1 < y0)
        {
            for (uint8_t y = y1; y <= y0; y++)
                gFrameBuffer[CW_LINE_DECODE][col] |= Y_TO_BIT(y);
        }
    }

    const uint8_t lastIdx = (uint8_t)((gCW_TraceHead + 127) & 0x7Fu);
    const uint8_t lastLevel = (gCW_TraceHistory[lastIdx] > gCW_TracePeak[lastIdx]) ? gCW_TraceHistory[lastIdx] : gCW_TracePeak[lastIdx];
    gFrameBuffer[CW_LINE_DECODE][127] |= Y_TO_BIT(LEVEL_TO_Y(lastLevel));

    #undef Y_TO_BIT
    #undef LEVEL_TO_Y
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void CW_Decoder_Init(void)
{
    if (gCW_DahMs != gCW_DitMs * 3)
        gCW_DahMs = gCW_DitMs * 3;
    CW_Decoder_Reset(true);
}

void CW_Decoder_Reset(bool clearDecodedText)
{
    if (clearDecodedText)
    {
        gCW_DecodeCursor = 0;
        gCW_DecodeText[0] = '\0';
        // Only reset noise floor on full reset (e.g. on start/stop).
        // Keep calibrated floor during partial resets (e.g. WPM change)
        // to avoid a brief burst of false marks while the floor re-calibrates.
        gCW_RxNoiseFloor = -120;
    }
    gCW_RxMorseLen = 0;
    gCW_RxMorse[0] = '\0';
    gCW_RxMarkTicks = 0;
    gCW_RxSpaceTicks = 0;
    gCW_RxSignalPrev = false;
    gCW_RxActive = false;
    gCW_RxActiveDebounce = 0;
    gCW_StartupDelay = true;
    memset(gCW_TraceHistory, 0, sizeof(gCW_TraceHistory));
    memset(gCW_TracePeak, 0, sizeof(gCW_TracePeak));
    gCW_RxToneState = false;
    gCW_RxToneOnDebounce = 0;
    gCW_RxToneOffDebounce = 0;
    gCW_RxTraceClock = 0;
    gCW_TraceHead = 0;
    gCW_RxDecoderState = CW_RX_STATE_IDLE;
    gCW_RxCurrentRssi = -120;
    gCW_RxSignalSum = 0;
    gCW_RxElementsInChar = 0;
    gCW_RxCharConfidence = 100;
}

// ---------------------------------------------------------------------------
// Main tick – called from CW_TimeSlice10ms() at 10 ms cadence
// ---------------------------------------------------------------------------
void CW_Decoder_ProcessTick(int16_t rssi_dBm)
{
    if (gCW_StartupDelay)
    {
        static uint8_t startupTicks = 0;
        if (startupTicks++ >= 50)
        {
            gCW_StartupDelay = false;
            startupTicks = 0;
        }
        return;
    }

    // 1. Detect tone
    CW_DetectTone(rssi_dBm);
    const bool toneNow = gCW_RxToneState;

    // 2. RX activation
    CW_HandleRxActivation(toneNow);
    CW_UpdateTraceBuffer(toneNow, rssi_dBm);

    // 3. Timing in ticks (1 tick = 10 ms)
    const uint16_t ditTicks = (uint16_t)MAX(1, (gCW_DitMs + 5) / 10);
    const uint16_t charGapTicks = (uint16_t)(3 * ditTicks);
    const uint16_t wordGapTicks = (uint16_t)(7 * ditTicks);

    // 4. State machine
    if (toneNow)
    {
        // ---- TONE ON ----
        if (gCW_RxSignalPrev)
        {
            // Continue existing mark
            if (gCW_RxMarkTicks < 0xFFFE)
                gCW_RxMarkTicks++;
            return;
        }

        // Transition: silence → tone
        // Check if previous gap was long enough for a word space
        if (gCW_RxDecoderState == CW_RX_STATE_GAP)
        {
            if (gCW_RxSpaceTicks >= wordGapTicks)
            {
                CW_AppendDecodedText(" ");
            }
        }

        gCW_RxSignalPrev = true;
        gCW_RxMarkTicks = 0;
        gCW_RxSpaceTicks = 0;
        gCW_RxDecoderState = CW_RX_STATE_MARK;
        return;
    }

    // ---- TONE OFF ----
    if (gCW_RxSignalPrev)
    {
        // Signal just fell: classify the mark that just ended
        if (gCW_RxMarkTicks > 0)
        {
            CW_ClassifyMark(gCW_RxMarkTicks, ditTicks);
            CW_PushTraceSample(CW_SIGNAL_THRESHOLD);
        }

        gCW_RxSignalPrev = false;
        gCW_RxMarkTicks = 0;
        gCW_RxSpaceTicks = 1;  // count the first gap tick
        gCW_RxDecoderState = CW_RX_STATE_GAP;
        return;
    }

    // ---- GAP: count silence, decode when threshold is reached ----
    if (gCW_RxDecoderState == CW_RX_STATE_GAP)
    {
        if (gCW_RxSpaceTicks < 0xFFFE)
            gCW_RxSpaceTicks++;

        if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= charGapTicks)
        {
            // Character gap reached: decode the accumulated Morse pattern.
            // Word spacing is handled below and by the RISE handler
            // (see "Transition: silence -> tone" block) when the next tone
            // arrives after a word-length gap.
            CW_DecodeCharacter(false);
            gCW_RxSpaceTicks = charGapTicks;  // prevent re-trigger
        }

        if (gCW_RxSpaceTicks >= wordGapTicks)
        {
            // Word gap reached.
            // If a character is still pending (e.g. gap arrived before char
            // threshold), decode it with space. Otherwise add space directly
            // for the word boundary. The RISE handler also adds word spaces
            // as a redundant safety net.
            if (gCW_RxMorseLen > 0)
                CW_DecodeCharacter(true);
            else
                CW_AppendDecodedText(" ");
            gCW_RxSpaceTicks = wordGapTicks;  // prevent re-trigger
            CW_Render();
        }
    }
}

// ---------------------------------------------------------------------------
// Status accessors
// ---------------------------------------------------------------------------
bool CW_Decoder_IsToneActive(void)
{
    return gCW_RxToneState;
}

bool CW_Decoder_IsRxActive(void)
{
    return gCW_RxActive;
}

const char *CW_Decoder_GetDecodedText(void)
{
    return gCW_DecodeText;
}

const char *CW_Decoder_GetCurrentMorse(void)
{
    return gCW_RxMorse;
}

uint8_t CW_Decoder_GetCharConfidence(void)
{
    return gCW_RxCharConfidence;  // computed from signal-to-noise ratio per character
}
