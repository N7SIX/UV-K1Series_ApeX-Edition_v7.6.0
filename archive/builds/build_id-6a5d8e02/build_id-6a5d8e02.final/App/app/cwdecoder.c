/* Copyright 2026 Sean, N7SIX
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
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

#define CW_RX_MORSE_MAX_LEN 10
#define CW_SIGNAL_THRESHOLD  32
#define CW_SIGNAL_FULL       64
#define CW_NOISE_FLOOR       20
#define CW_TRACE_BUF_SIZE 128

#define CW_RX_DEBOUNCE_TICKS   1
#define CW_RX_ACTIVATE_TICKS   5
#define CW_RX_DEACTIVATE_TICKS 3
#define CW_MULTI_TAP_TIMEOUT_TICKS 80


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
static int16_t  gCW_PeakRssi = -110;
static int16_t  gCW_RxNoiseFloor = -120;
static int16_t  gCW_RxSignalFloor = -110;
static bool     gCW_StartupDelay = false;
static uint8_t  gCW_TraceHistory[CW_TRACE_BUF_SIZE];
static uint8_t  gCW_TracePeak[CW_TRACE_BUF_SIZE];
static uint8_t  gCW_TraceHead = 0;
static uint8_t  gCW_CharConfidence = 0;
static uint16_t gCW_CharSignalSum = 0;
static uint16_t gCW_CharSignalCount = 0;

typedef enum {
    CW_RX_STATE_IDLE = 0,
    CW_RX_STATE_MARK,
    CW_RX_STATE_GAP
} CW_RxDecoderState_t;

static CW_RxDecoderState_t gCW_RxDecoderState = CW_RX_STATE_IDLE;

// Flattened binary tree lookup (Index 0 unused)
// Left child = 2*index (Dot), Right child = 2*index+1 (Dash)
// Covers A-Z, 0-9. Punctuation falls back to array scan.
//
// Tree structure (depth-first, left = dot, right = dash):
//   Level 1:  root
//   Level 2:  E  T
//   Level 3:  I A N M
//   Level 4:  S U R W D K G O
//   Level 5:  H V F * L * P J B X C Y Z Q * *
//   Level 6:  5 4 * 3 * * * 2 * * + * * * * 1
//             6 = / * * * * * * 7 * * * * 8 * 9 0
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

static void CW_PushTraceSample(uint8_t level)
{
    gCW_TraceHistory[gCW_TraceHead] = level;
    if (level > gCW_TracePeak[gCW_TraceHead])
        gCW_TracePeak[gCW_TraceHead] = level;
    else if (gCW_TracePeak[gCW_TraceHead] > 0)
        gCW_TracePeak[gCW_TraceHead]--;

    gCW_TraceHead = (gCW_TraceHead + 1) & 0x7F;
}

// Literal manual translation: record "dit" for short marks, "dah" for long marks.
// Threshold at 2×dit keeps simple timing without adaptive confidence gating.
static void CW_ClassifyMark(uint16_t markTicks, uint16_t ditTicks)
{
    if (markTicks < MAX(2U, (ditTicks + 1U) / 2U))
        return; // too short — ignore

    if (gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
    {
        if (markTicks >= (ditTicks * 2U))
            gCW_RxMorse[gCW_RxMorseLen++] = '-';
        else
            gCW_RxMorse[gCW_RxMorseLen++] = '.';
        gCW_RxMorse[gCW_RxMorseLen] = '\0';
        CW_Render();  // Update display when Morse pattern changes
    }
}

static void CW_FinalizeRxCharacter(void)
{
    if (gCW_RxMorseLen == 0)
        return;

    // Calculate confidence based on signal quality during this character
    uint8_t confidence = 0;
    if (gCW_CharSignalCount > 0)
    {
        // Average signal quality: 0-255 scale, higher is better
        uint16_t avgQuality = (gCW_CharSignalSum + gCW_CharSignalCount / 2) / gCW_CharSignalCount;
        // Map to 0-100 confidence: require at least 3 dB SNR, max at 20 dB
        // Lowered from 8 dB to 3 dB to avoid rejecting valid characters when
        // the BK4819 AGC hold (~-90 dBm) keeps the noise floor estimate elevated.
        if (avgQuality >= 3)
        {
            confidence = (uint8_t)((avgQuality - 3) * 100 / 17);
            if (confidence > 100) confidence = 100;
        }
    }
    
    // Adaptive confidence threshold: shorter characters need lower threshold
    // because they have fewer signal samples. Scale from 5% (1 element) to 15% (10 elements).
    // Lowered from 20-25 to 5-15 to prevent rejecting characters with marginal SNR
    // that are still perfectly decodable by the Morse tree.
    uint8_t minConfidence = (uint8_t)(5 + (gCW_RxMorseLen * 10) / 10);
    if (minConfidence > 15) minConfidence = 15;
    if (minConfidence < 5) minConfidence = 5;
    
    // Only accept character if confidence is sufficient (adaptive threshold)
    bool acceptChar = (confidence >= minConfidence);
    
    gCW_RxMorse[gCW_RxMorseLen] = '\0';
    const char *decodedToken = CW_MorseToDecodedToken(gCW_RxMorse);
    if (acceptChar && decodedToken != NULL && decodedToken[0] != '\0')
    {
        gCW_CharConfidence = confidence;
        CW_AppendDecodedText(decodedToken);
    }

    gCW_RxMorseLen = 0;
    gCW_CharSignalSum = 0;
    gCW_CharSignalCount = 0;
    CW_Render();  // Update display when character is decoded
}

static void CW_IsRxTonePresent(int16_t rssi_dBm)
{
    // Update confidence based on signal quality
    if (gCW_RxToneState && rssi_dBm > gCW_RxNoiseFloor)
    {
        // Signal present and above noise - increase confidence
        const uint8_t quality = (uint8_t)(rssi_dBm - gCW_RxNoiseFloor);
        gCW_CharSignalSum += quality;
        gCW_CharSignalCount++;
    }
    
    if (rssi_dBm > gCW_PeakRssi)
        gCW_PeakRssi = rssi_dBm;
    else if (rssi_dBm < (gCW_PeakRssi - 10))
        gCW_PeakRssi = (int16_t)(gCW_PeakRssi - 1);
    if (gCW_PeakRssi < -110) gCW_PeakRssi = -110;

    if (rssi_dBm > gCW_RxSignalFloor)
        gCW_RxSignalFloor = (int16_t)((gCW_RxSignalFloor * 3 + rssi_dBm + 2) / 4);
    else
        gCW_RxSignalFloor = (int16_t)((gCW_RxSignalFloor * 7 + rssi_dBm + 4) / 8);

    // Only update noise floor when tone is NOT present to avoid tracking the signal.
    // Cap at -95 dBm to prevent drift toward AGC hold level (~-90 dBm) during gaps.
    // Without this cap, noise floor rises from -120 to -90 dBm over a few seconds,
    // reducing SNR and causing characters to be dropped by the confidence threshold.
    if (!gCW_RxToneState)
    {
        if (rssi_dBm < gCW_RxNoiseFloor)
            gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 7 + rssi_dBm + 4) / 8);
        else
            gCW_RxNoiseFloor = (int16_t)((gCW_RxNoiseFloor * 15 + rssi_dBm + 8) / 16);
        if (gCW_RxNoiseFloor > -97)
            gCW_RxNoiseFloor = -97;
    }

    // Simple fixed-threshold approach for BK4819.
    // BK4819 RSSI behavior:
    //   - Carrier present: ~-60 dBm
    //   - Carrier off (AGC hold): ~-90 dBm during short gaps
    //   - Noise floor: ~-120 dBm
    // Open at 75% of signal-to-noise span (adapts to noise level).
    // Close at fixed -80 dBm (~midway between AGC hold and carrier peak).
    // This reliably separates elements because AGC hold (~-90 dBm) is below -80 dBm.
    const int16_t signalSpan = (int16_t)MAX(4, (gCW_RxSignalFloor - gCW_RxNoiseFloor));
    const int16_t effectiveOpenLevel = (int16_t)MAX(
        gCW_RxNoiseFloor + (signalSpan * 3) / 4 + 3, (int16_t)-85);
    // -80 dBm chosen as close threshold: above AGC-hold dropout (~-90 dBm) but
    // well below carrier peak (~-60 dBm). This prevents chatter during AGC holes.
    const int16_t effectiveCloseLevel = (int16_t)-80;

    bool signalStrong;
    if (!gCW_RxToneState)
        signalStrong = (rssi_dBm >= effectiveOpenLevel) && (rssi_dBm >= (gCW_RxNoiseFloor + 2));
    else
        signalStrong = (rssi_dBm >= effectiveCloseLevel);

    if (signalStrong)
    {
        if (gCW_RxToneOnDebounce < 0xFF) gCW_RxToneOnDebounce++;
        gCW_RxToneOffDebounce = 0;
        if (!gCW_RxToneState && gCW_RxToneOnDebounce >= CW_RX_DEBOUNCE_TICKS)
        {
            gCW_RxToneState = true;
        }
    }
    else
    {
        if (gCW_RxToneOffDebounce < 0xFF) gCW_RxToneOffDebounce++;
        gCW_RxToneOnDebounce = 0;
        if (gCW_RxToneState && gCW_RxToneOffDebounce >= CW_RX_DEBOUNCE_TICKS)
        {
            gCW_RxToneState = false;
        }
    }
}

static void CW_HandleRxActivation(bool signalNow)
{
    if (signalNow && !gCW_RxActive)
    {
        if (gCW_RxActiveDebounce < 0xFF) gCW_RxActiveDebounce++;
        if (gCW_RxActiveDebounce >= CW_RX_ACTIVATE_TICKS)
        {
            gCW_RxActive = true;
            gCW_RxActiveDebounce = 0;
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
        }
    }
    else if (!signalNow && !gCW_RxActive)
    {
        gCW_RxActiveDebounce = 0;
        gCW_RxInactiveDebounce = 0;
    }
}

static void CW_UpdateTraceBuffer(bool signalNow, int16_t rssi_dBm)
{
    if (!gCW_RxActive)
    {
        if (gCW_RxTraceClock == 0) CW_PushTraceSample(0);
        gCW_RxTraceClock = (gCW_RxTraceClock + 1) & 1u;
    }
    else
    {
        const bool traceActive = signalNow || gCW_RxSignalPrev;
        const uint16_t ditTicks = (uint16_t)MAX(1, (gCW_DitMs + 5) / 10);
        const uint16_t traceAdvanceTicks = traceActive ? 1U : MAX(2U, (ditTicks + 1U) / 2U);
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

void CW_Decoder_Init(void)
{
    // gCW_DitMs was already set by CW_UpdateTiming() in CW_Init()
    // Just derive dah timing from it, replacing the hardcoded 60/180
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
    gCW_PeakRssi = -110;
    gCW_RxNoiseFloor = -120;
    gCW_RxSignalFloor = -110;
    gCW_RxToneState = false;
    gCW_RxDecoderState = CW_RX_STATE_IDLE;
    gCW_RxToneOnDebounce = 0;
    gCW_RxToneOffDebounce = 0;
    gCW_RxTraceClock = 0;
    gCW_TraceHead = 0;
    gCW_CharConfidence = 0;
    gCW_CharSignalSum = 0;
    gCW_CharSignalCount = 0;
}

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

    // --- Tone detection (same tick throughout) ---
    CW_IsRxTonePresent(rssi_dBm);
    const bool toneNow = gCW_RxToneState;
    CW_HandleRxActivation(toneNow);
    CW_UpdateTraceBuffer(toneNow, rssi_dBm);

    // --- Timing derived from WPM setting only ---
    const uint16_t ditTicks = (uint16_t)MAX(1, (gCW_DitMs + 5) / 10);
    const uint16_t charGapTicks = (uint16_t)(3 * ditTicks);
    const uint16_t wordGapTicks = (uint16_t)(7 * ditTicks);

    // --- Signal RISE (tone ON, was OFF → start timing a mark) ---
    if (toneNow)
    {
        if (gCW_RxSignalPrev)
        {
            // Continue counting an existing mark
            if (gCW_RxMarkTicks < 0xFFFE)
                gCW_RxMarkTicks++;
            return;
        }

        // Transition: was silent, now tone → add word space if gap was long enough
        if (gCW_RxDecoderState == CW_RX_STATE_GAP)
        {
            if (gCW_RxSpaceTicks >= wordGapTicks)
            {
                if (gCW_DecodeCursor == 0 || gCW_DecodeText[gCW_DecodeCursor - 1] != ' ')
                    CW_AppendDecodedText(" ");
            }
        }

        // Start new mark
        gCW_RxSignalPrev = true;
        gCW_RxMarkTicks = 0;  // Start from 0 for accurate timing
        gCW_RxSpaceTicks = 0;
        gCW_RxDecoderState = CW_RX_STATE_MARK;
        return;
    }

    // --- Signal FALL (tone OFF, was ON → classify the mark) ---
    if (gCW_RxSignalPrev)
    {
        if (gCW_RxMarkTicks > 0 && gCW_RxMorseLen < CW_RX_MORSE_MAX_LEN)
        {
            // Adaptive hysteresis already validated signal presence; classify directly
            CW_ClassifyMark(gCW_RxMarkTicks, ditTicks);
            CW_PushTraceSample(CW_SIGNAL_THRESHOLD);
        }

        // Finalize character immediately on signal fall if gap threshold met
        // Single-element characters (T, E) need a longer gap to avoid
        // premature finalization when inter-element gaps are slightly long.
        uint16_t effectiveCharGap = (gCW_RxMorseLen == 1) ? 
            (uint16_t)(charGapTicks * 2) : charGapTicks;
        if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= effectiveCharGap)
        {
            CW_FinalizeRxCharacter();
        }

        gCW_RxSignalPrev = false;
        gCW_RxMarkTicks = 0;
        gCW_RxSpaceTicks = 1;
        gCW_RxDecoderState = CW_RX_STATE_GAP;
        return;
    }

    // --- GAP (tone OFF, was OFF → accumulate space, check for char/word boundaries) ---
    if (gCW_RxDecoderState == CW_RX_STATE_GAP)
    {
        if (gCW_RxSpaceTicks < 0xFFFE)
            gCW_RxSpaceTicks++;

        // Single-element characters (T, E) need a longer gap to avoid
        // premature finalization when inter-element gaps are slightly long.
        uint16_t effectiveCharGap = (gCW_RxMorseLen == 1) ? 
            (uint16_t)(charGapTicks * 2) : charGapTicks;
        if (gCW_RxMorseLen > 0 && gCW_RxSpaceTicks >= effectiveCharGap)
        {
            CW_FinalizeRxCharacter();
            gCW_RxSpaceTicks = effectiveCharGap;  // prevent re-finalization
        }

        // Word gap detection: add space based on gap duration alone (MorseLen may
        // have been cleared by CW_FinalizeRxCharacter above).
        if (gCW_RxSpaceTicks >= wordGapTicks)
        {
            if (gCW_DecodeCursor == 0 || gCW_DecodeText[gCW_DecodeCursor - 1] != ' ')
                CW_AppendDecodedText(" ");
            gCW_RxSpaceTicks = wordGapTicks - 1;  // Prevent re-trigger in RISE handler
            CW_Render();  // Update display when word space is added
        }
    }
}

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
    return gCW_CharConfidence;
}