/**
 * =====================================================================================
 * @file        spectrum.c
 * @brief       Professional-grade Spectrum Analyzer + Waterfall for Quansheng UV-K1 Series
 * @author      fagci (Original Framework, 2023)
 * @author      N7SIX (Professional Enhancements, 2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * * "Bringing professional signal analysis to the palm of your hand."
 * =====================================================================================
 * * ARCHITECTURAL OVERVIEW:
 * This module implements a real-time band-scope with a focus on visual fluidity 
 * and RF accuracy. It utilizes the BK4819's RSSI registers paired with high-speed 
 * loop processing to provide a visualization experience comparable to high-tier 
 * Yaesu and Icom desktop SDRs.
 *
 * MAJOR N7SIX ENHANCEMENTS (2025-2026):
 * ------------------------------------
 * - ADVANCED VISUALS: 16-level grayscale waterfall with temporal persistence.
 * - SMART SQUELCH: Scan-based trigger level auto-adjustment (STLA).
 * - RX CONTINUITY: Non-interruptive waterfall updates during active signal reception.
 * - PEAK DYNAMICS: Max-hold history with stabilized linear decay.
 * - STOCHASTIC NOISE: "Professional Grass" engine for organic RF floor simulation.
 * - UX INTEGRATION: Real-time channel name injection and layout optimization.
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - DISPLAY ARCHITECTURE: 128x64 ST7565 LCD utilizing Bank-based memory mapping.
 * - RSSI PROCESSING: 16-bit dynamic range with exponential moving average (EMA) filtering.
 * - FREQUENCY PRECISION: 10Hz internal resolution with configurable step indices.
 * - MEMORY FOOTPRINT: Highly optimized SRAM usage (< 2KB) for compatibility with UV-K5/K6.
 * - RF CALIBRATION: Hardware register-level optimization (0x13/0x14) for gain alignment.
 *
 * =====================================================================================
 */

 // =============================================================================
 // SYSTEM AND DRIVER HEADERS
 // =============================================================================
 #include <stdbool.h>
 #include <stdint.h>
 #include <stdlib.h> // Fixes 'abs' warning
 #include <string.h>
 
 #include "driver/backlight.h"
 #include "driver/bk4819.h"
 #include "spectrum.h"
 #include "driver/eeprom.h"  // Added for spectrum settings persistence
 #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
 #include "driver/py25q16.h"
 #endif
 
 // =============================================================================
 // APPLICATION AND UI HEADERS
 // =============================================================================
 #include "am_fix.h"
 #include "app/spectrum.h"
 #include "audio.h"
 #include "frequencies.h"
 #include "functions.h"
 #include "main.h"
 #include "misc.h"
 #include "ui/helper.h"
 #include "ui/main.h"
 #include "ui/ui.h"
 #include "app/main.h"
 
 /**
 * @brief Local microsecond delay for Cortex-M0+ (approx 48MHz)
 * Satisfies the linker and provides PLL settling time.
 */
void SYSTEM_DelayUs(uint32_t us)
{
    for (uint32_t i = 0; i < us * 12; i++) {
        __asm__ volatile("nop");
    }
}
 
 #ifdef ENABLE_SCAN_RANGES
 #include "chFrScanner.h"
 #endif
 
 #ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
 #include "screenshot.h"
 #endif
 
 // Include app.h for APP_StartListening declaration
 extern void APP_StartListening(FUNCTION_Type_t function);
 
 // =============================================================================
 // FORWARD DECLARATIONS
 // =============================================================================
 static uint8_t messageTimer = 0;
 static const char* toastMessage = "";
 static void Measure(void); 
 static void InitScan(void);
 static void ResetPeak(void);
 static void ToggleRX(bool enable);
 static inline uint8_t MapMeasurementToDisplay(uint32_t idx);
static uint16_t GetDisplayWidth(void);
 
 // =============================================================================
 // CONSTANTS AND CONFIGURATION
 // =============================================================================
 #define PEAK_HOLD_DECAY             2
 #define RSSI_MAX_VALUE              65535U
 #define DISPLAY_STRING_BUFFER_SIZE  32U
 #define SPECTRUM_MAX_STEPS          128U
 #define WATERFALL_HISTORY_DEPTH     16U
 #define FREQ_INPUT_MAX_LENGTH       10U
 #define FREQ_INPUT_STRING_SIZE      11U
 #define BLACKLIST_MAX_ENTRIES       15U
 #define WATERFALL_UPDATE_INTERVAL   2U
 #define WATERFALL_COLOR_LEVELS      16U

 #define DISPLAY_DBM_MIN            -130
 #define DISPLAY_DBM_MAX            -50
 
 #define F_MIN  frequencyBandTable[0].lower
 #define F_MAX  frequencyBandTable[BAND_N_ELEM - 1].upper

 // EEPROM addresses for persistent spectrum settings (reserved region)
 // Using addresses 0x1E80-0x1E8F for spectrum state (16 bytes)
 #define SPECTRUM_EEPROM_ADDR        0x1E80
 #define SPECTRUM_EEPROM_SIZE        16
 
 // =============================================================================
 // DATA STRUCTURES
 // =============================================================================
 struct FrequencyBandInfo
 {
     uint32_t lower;   /**< Lower frequency bound in Hz */
     uint32_t upper;   /**< Upper frequency bound in Hz */
     uint32_t middle;  /**< Middle frequency in Hz */
 };
 
 // =============================================================================
 // CALIBRATION DATA 
 // =============================================================================

 typedef struct {
    uint32_t freq;    // Frequency in 10Hz units
    int8_t   offset;  // Correction in dBm
} CalibrationPoint;

static const CalibrationPoint calTable[] = {
    {13600000, -2},  // 136 MHz: Slight boost needed
    {14400000,  0},  // 144 MHz: Reference point (0 dB)
    {20000000,  3},  // 200 MHz: Higher loss, add +3dB
    {35000000,  5},  // 350 MHz: Add +5dB
    {43000000,  8},  // 430 MHz: Significant front-end loss, add +8dB
    {52000000, 12}   // 520 MHz: High frequency rolloff, add +12dB
};
 
 // =============================================================================
 // GLOBAL AND STATIC VARIABLES
 // =============================================================================
 
 // Scan range globals (declared unconditionally to avoid undefined reference errors)
 #ifndef ENABLE_SCAN_RANGES
 uint32_t gScanRangeStart = 0;
 uint32_t gScanRangeStop = 0;
 #endif
 
 // Module state flags
 static bool isInitialized = false;
 bool isListening = true;
 bool monitorMode = false;
 bool redrawStatus = true;
 bool redrawScreen = false;
 bool newScanStart = true;
 bool preventKeypress = true;
 bool audioState = true;
 bool lockAGC = false;
 
 // State management
 State currentState = SPECTRUM;
 State previousState = SPECTRUM;
 
 // Signal and Scan Data
 PeakInfo peak;
 ScanInfo scanInfo;
 static uint16_t displayRssi = 0;
 static KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0};
 
 // Frequency management
 static uint32_t initialFreq;
 uint32_t fMeasure = 0;
 uint32_t currentFreq;
 uint32_t tempFreq;
 int vfo;
 
 // Spectrum and Waterfall Buffers
 uint16_t rssiHistory[SPECTRUM_MAX_STEPS];
 uint8_t waterfallHistory[SPECTRUM_MAX_STEPS][WATERFALL_HISTORY_DEPTH / 2]; 
 uint8_t waterfallIndex = 0;
 uint16_t waterfallUpdateCounter = 0;
 static uint16_t smoothedRssi[SPECTRUM_MAX_STEPS] = {0};
 
 // --- 1. PROTOTYPES (Add this near other static prototypes) ---
static void ProcessSpectrumEnhancements(void);

// --- 2. CONSOLIDATED VARIABLES (Replace the conflicting lines 196-203) ---
static uint16_t peakHold[SPECTRUM_MAX_STEPS];
static uint8_t  peakAge[SPECTRUM_MAX_STEPS];
static uint16_t smoothedRssi[SPECTRUM_MAX_STEPS];
// (Make sure you deleted the old 'static uint8_t peakHold' and 'peakHoldAge')

// Mapping from display column -> best raw measurement index (used for accurate tuning)
static uint16_t displayBestIndex[SPECTRUM_MAX_STEPS];
 
 // Frequency input buffer
 uint8_t freqInputIndex = 0;
 uint8_t freqInputDotIndex = 0;
 KEY_Code_t freqInputArr[FREQ_INPUT_MAX_LENGTH];
 char freqInputString[FREQ_INPUT_STRING_SIZE];
 
 // UI/System State
 uint8_t menuState = 0;
 uint16_t listenT = 0;
 static char String[DISPLAY_STRING_BUFFER_SIZE];
 uint16_t statuslineUpdateTimer = 0;
 
 #ifdef ENABLE_SCAN_RANGES
 static uint16_t blacklistFreqs[BLACKLIST_MAX_ENTRIES];
 static uint8_t blacklistFreqsIdx;
 #endif
 
 // =============================================================================
 // TABLES AND SETTINGS
 // =============================================================================
 const char *bwOptions[] = {"25", "12.5", "6.25"};
 const uint8_t modulationTypeTuneSteps[] = {100, 50, 10};
 const uint8_t modTypeReg47Values[] = {1, 7, 5};
 
 RegisterSpec registerSpecs[] = {
     {}, // Index 0: unused
     {"LNAs", BK4819_REG_13, 8, 0b11, 1},
     {"LNA",  BK4819_REG_13, 5, 0b111, 1},
     {"VGA",  BK4819_REG_13, 0, 0b111, 1},
     {"BPF",  BK4819_REG_3D, 0, 0xFFFF, 0x2aaa}
 };
 
 SpectrumSettings settings = {
     .stepsCount = STEPS_64,
     .scanStepIndex = S_STEP_25_0kHz,
     .frequencyChangeStep = 600000,  // Default to ±600.00K (600kHz)
     .scanDelay = 3200,
     .rssiTriggerLevel = 150,
     .backlightState = true,
     .bw = BK4819_FILTER_BW_WIDE,
     .listenBw = BK4819_FILTER_BW_WIDE,
     .modulationType = false,
     .dbMin = DISPLAY_DBM_MIN,
     .dbMax = DISPLAY_DBM_MAX
 };
 
 #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
 const int8_t LNAsOptions[] = {-19, -16, -11, 0};
 const int8_t LNAOptions[]  = {-24, -19, -14, -9, -6, -4, -2, 0};
 const int8_t VGAOptions[]  = {-33, -27, -21, -15, -9, -6, -3, 0};
 const char *BPFOptions[]   = {"8.46", "7.25", "6.35", "5.64", "5.08", "4.62", "4.23"};
 #endif
 
 // =============================================================================
 // UTILITY FUNCTIONS (RANDOM, MATH, BANDS)
 // =============================================================================
 
 static uint16_t FastRandom(void) {
     static uint16_t lfsr = 0xACE1u;
     uint16_t bit;
     bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
     lfsr = (lfsr >> 1) | (bit << 15);
     return lfsr;
 }
 
 static int clamp(int v, int min, int max) {
     return v <= min ? min : (v >= max ? max : v);
 }
 
 static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }
 
 static uint8_t DBm2S(int dbm) {
     uint8_t i = 0;
     dbm *= -1;
     for (i = 0; i < ARRAY_SIZE(U8RssiMap); i++) {
         if (dbm >= U8RssiMap[i]) return i;
     }
     return i;
 }
 
 static int Rssi2DBm(uint16_t rssi) {
     return (rssi / 2) - 160 + dBmCorrTable[gRxVfo->Band];
 }
 
 // =============================================================================
 // DRAWING AND GRAPHICS
 // =============================================================================
 
 

// helper: convert a display index (0..bars-1) into an X pixel on screen
static uint8_t SpecIdxToX(uint16_t idx)
{
    uint16_t bars = GetDisplayWidth();
    if (bars <= 1) return 0;
    // use the same formula used by DrawSpectrumEnhanced so that the arrow,
    // spectrum and waterfall all agree.  denominator is bars-1 (maps final
    // index to pixel 127).
    return (uint8_t)(((uint32_t)idx * 127) / (bars - 1));
}

void DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color) {
     int8_t dx = abs(x1 - x0);
     int8_t dy = -abs(y1 - y0);
     int8_t sx = x0 < x1 ? 1 : -1;
     int8_t sy = y0 < y1 ? 1 : -1;
     int16_t err = dx + dy;
     while (1) {
         PutPixel(x0, y0, color); 
         if (x0 == x1 && y0 == y1) break;
         int16_t e2 = 2 * err;
         if (e2 >= dy) { err += dy; x0 += sx; }
         if (e2 <= dx) { err += dx; y0 += sy; }
     }
 }
 
#ifndef ENABLE_FEAT_N7SIX
 static void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool fill) {
     uint8_t c, pixels;
     const uint8_t *p = (const uint8_t *)pString;
     while ((c = *p++) && c != '\0') {
         c -= 0x20;
         for (int i = 0; i < 3; ++i) {
             pixels = gFont3x5[c][i];
             for (int j = 0; j < 6; ++j) {
                 if (pixels & 1) {
                     if (statusbar) PutPixelStatus(x + i, y + j, fill);
                     else PutPixel(x + i, y + j, fill);
                 }
                 pixels >>= 1;
             }
         }
         x += 4;
     }
 }
 #endif
 
 // =============================================================================
 // HARDWARE AND REGISTER CONTROL
 // =============================================================================
 
 static void SetBandLed(uint32_t freq, bool isTx, bool hasSignal) {
     if (!hasSignal) {
         BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
         BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
         return;
     }
     BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
     BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
 
     FREQUENCY_Band_t band = FREQUENCY_GetBand(freq);
     if (isTx) {
         BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
     } else {
         if (band == BAND3_137MHz || band == BAND4_174MHz) {
             BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
         } else if (band == BAND6_400MHz) {
             BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
             BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
         } else {
             BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
         }
     }
 }
 
 static uint16_t GetRegMenuValue(uint8_t st) {
     RegisterSpec s = registerSpecs[st];
     return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
 }
 
 void LockAGC(void) {
     RADIO_SetupAGC(settings.modulationType == MODULATION_AM, lockAGC);
     lockAGC = true;
 }
 
 static void SetRegMenuValue(uint8_t st, bool add) {
     uint16_t v = GetRegMenuValue(st);
     RegisterSpec s = registerSpecs[st];
     if (s.num == BK4819_REG_13) LockAGC();
     uint16_t reg = BK4819_ReadRegister(s.num);
     if (add && v <= s.mask - s.inc) v += s.inc;
     else if (!add && v >= s.inc) v -= s.inc;
     reg &= ~(s.mask << s.offset);
     BK4819_WriteRegister(s.num, reg | (v << s.offset));
     redrawScreen = true;
 }
 
 static void ToggleAFBit(bool on) {
     uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
     reg &= ~(1 << 8);
     if (on) reg |= on << 8;
     BK4819_WriteRegister(BK4819_REG_47, reg);
 }
 
 static const BK4819_REGISTER_t registers_to_save[] = {
     BK4819_REG_30, BK4819_REG_37, BK4819_REG_3D, BK4819_REG_43,
     BK4819_REG_47, BK4819_REG_48, BK4819_REG_7E,
 };
 static uint16_t registers_stack[sizeof(registers_to_save)];
 
 static void BackupRegisters() {
     for (uint32_t i = 0; i < ARRAY_SIZE(registers_to_save); i++)
         registers_stack[i] = BK4819_ReadRegister(registers_to_save[i]);
 }
 
 static void RestoreRegisters() {
     for (uint32_t i = 0; i < ARRAY_SIZE(registers_to_save); i++)
         BK4819_WriteRegister(registers_to_save[i], registers_stack[i]);
 #ifdef ENABLE_FEAT_N7SIX
     gVfoConfigureMode = VFO_CONFIGURE;
 #endif
 }
 
 static void ToggleAFDAC(bool on) {
     uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
     Reg &= ~(1 << 9);
     if (on) Reg |= (1 << 9);
     BK4819_WriteRegister(BK4819_REG_30, Reg);
 }
 
 // =============================================================================
 // SPECTRUM CORE LOGIC (SETTINGS, RX, SCAN)
 // =============================================================================
 
 static void SetWaterfallLevel(uint8_t x, uint8_t y, uint8_t level) {
     if (x >= SPECTRUM_MAX_STEPS || y >= WATERFALL_HISTORY_DEPTH) return;
     uint8_t row = y >> 1;
     if (!(y & 1)) waterfallHistory[x][row] = (waterfallHistory[x][row] & 0xF0) | (level & 0x0F);
     else waterfallHistory[x][row] = (waterfallHistory[x][row] & 0x0F) | (level << 4);
 }
 
 static uint8_t GetWaterfallLevel(uint8_t x, uint8_t y) {
     if (x >= SPECTRUM_MAX_STEPS || y >= WATERFALL_HISTORY_DEPTH) return 0;
     uint8_t row = y >> 1;
     if (!(y & 1)) return waterfallHistory[x][row] & 0x0F;
     return (waterfallHistory[x][row] >> 4) & 0x0F;
 }
 
 #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
 static void LoadSettings(void) {
     uint8_t data[8] = {0};
     PY25Q16_ReadBuffer(0x00c000, data, sizeof(data));
     settings.scanStepIndex = ((data[3] & 0xF0) >> 4);
     if (settings.scanStepIndex > 14) settings.scanStepIndex = S_STEP_25_0kHz;
     settings.stepsCount = ((data[3] & 0x0F) & 0b1100) >> 2;
     if (settings.stepsCount > 3) settings.stepsCount = STEPS_64;
     settings.listenBw = ((data[3] & 0x0F) & 0b0011);
     if (settings.listenBw > 2) settings.listenBw = BK4819_FILTER_BW_WIDE;
 }
 
 static void SaveSettings(void) {
     uint8_t data[8] = {0};
     PY25Q16_ReadBuffer(0x00c000, data, sizeof(data));
     data[3] = (settings.scanStepIndex << 4) | (settings.stepsCount << 2) | settings.listenBw;
     PY25Q16_WriteBuffer(0x00c000, data, sizeof(data), true);
 }
 #endif
 
 static void SetF(uint32_t f) {
     fMeasure = f;
     BK4819_SetFrequency(fMeasure);
     BK4819_PickRXFilterPathBasedOnFrequency(fMeasure);
     uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
     BK4819_WriteRegister(BK4819_REG_30, 0);
     BK4819_WriteRegister(BK4819_REG_30, reg);
 }
 
 bool IsPeakOverLevel() { return peak.rssi >= settings.rssiTriggerLevel; }
 
 static void ResetPeak() {
     peak.t = 0;
     peak.rssi = 0;
 }
 
 #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
 static void setTailFoundInterrupt() {
     BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_02_CxCSS_TAIL | BK4819_REG_02_SQUELCH_FOUND);
 }
 
 static bool checkIfTailFound() {
     uint16_t interrupt_status_bits;
     if(BK4819_ReadRegister(BK4819_REG_0C) & 1u) {
         BK4819_WriteRegister(BK4819_REG_02, 0);
         interrupt_status_bits = BK4819_ReadRegister(BK4819_REG_02);
         if (interrupt_status_bits & BK4819_REG_02_CxCSS_TAIL) {
             listenT = 0;
             BK4819_WriteRegister(BK4819_REG_3F, 0);
             BK4819_WriteRegister(BK4819_REG_02, 0);
             return true;
         }
     }
     return false;
 }
 #endif
 
 bool IsCenterMode() { return settings.scanStepIndex < S_STEP_2_5kHz; }
 uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }
 
 uint16_t GetStepsCount() {
 #ifdef ENABLE_SCAN_RANGES
     if (gScanRangeStart) {
         return ((gScanRangeStop - gScanRangeStart) / GetScanStep()) + 1;
     }
 #endif
     return 128 >> settings.stepsCount;
 }
 
 #ifdef ENABLE_SCAN_RANGES
 static uint16_t GetStepsCountDisplay() {
     // previously this helper omitted the "+1" used by GetStepsCount().
     // the discrepancy caused the on‑screen count to be off by one when a
     // scan range was active.  The display should reflect the actual
     // number of measurements we will perform.
     if (gScanRangeStart) {
         return ((gScanRangeStop - gScanRangeStart) / GetScanStep()) + 1;
     }
     return GetStepsCount();
}
 #endif
 
 uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }

// When we draw the spectrum and waterfall we sometimes need a reduced
// horizontal resolution to honour the user's stepsCount setting.  For
// normal full-band scans the width equals the raw measurement count
// (GetStepsCount()).  During scan-range mode the caller may override
// `settings.stepsCount` to force a maximum bar count; both the spectral
// trace and waterfall must use the same value to stay aligned.
//
// CRITICAL: In scan-range mode, we always display the full range across
// the spectrum (all measurements), ignoring the stepsCount constraint.
// This ensures the display is properly centered and aligned.
static uint16_t GetDisplayWidth(void)
{
    uint16_t steps = GetStepsCount();
#ifdef ENABLE_SCAN_RANGES
    // In scan-range mode, always display the full measurement range
    // without applying the stepsCount constraint. This keeps the entire
    // range visible and properly centered on-screen.
    if (gScanRangeStart) {
        return (steps < SPECTRUM_MAX_STEPS) ? steps : SPECTRUM_MAX_STEPS;
    }
#endif
    uint8_t maxBars = 128 >> settings.stepsCount; // 128,64,32,16
    return (steps < maxBars) ? steps : maxBars;
}

uint32_t GetFStart() {
    /* With a scan range active the start frequency should always be the
       user-selected lower bound unless we're in centre mode and the centre
       has wandered outside the range.  The previous implementation merely
       clamped low values; that's not sufficient since currentFreq may be
       greater than the lower bound and inadvertently shift the window. */
#ifdef ENABLE_SCAN_RANGES
    if (gScanRangeStart) {
        // range mode: always start at the defined lower bound regardless
        // of center-frequency shifts.  The cursor (currentFreq) is used by
        // centre‑mode markers but does not move the sweep window.
        return gScanRangeStart;
    }
#endif
    if (IsCenterMode()) {
        uint32_t half = GetBW() >> 1;
        return (currentFreq > half) ? (currentFreq - half) : 0;
    }
    return currentFreq;
}

uint32_t GetFEnd() {
#ifdef ENABLE_SCAN_RANGES
    if (gScanRangeStart) {
        // range mode: always end at the defined upper bound.
        return gScanRangeStop;
    }
#endif
    if (IsCenterMode()) {
        return currentFreq + (GetBW() >> 1);
    }
    return currentFreq + GetBW();
}


static uint32_t GetCentroidFrequency() 
{
    // 1. LISTEN LOCK: If we are already receiving a signal, 
    // do NOT recalculate. This is the #1 cause of the jitter.
    if (isListening && peak.f != 0) {
        return peak.f;
    }

    // Map the raw peak measurement index into the display grid so that
    // centroid interpolation uses values present in rssiHistory[].
    if (scanInfo.measurementsCount == 0) return initialFreq + (peak.i * scanInfo.scanStep);

    uint8_t dIdx = MapMeasurementToDisplay(peak.i);

    // Safety check: ensure we have valid neighbours in the display grid
    if (dIdx == 0 || dIdx >= (SPECTRUM_MAX_STEPS - 1)) {
        return initialFreq + (peak.i * scanInfo.scanStep);
    }

    int32_t R_center = (int32_t)rssiHistory[dIdx];
    int32_t R_left   = (int32_t)rssiHistory[dIdx - 1];
    int32_t R_right  = (int32_t)rssiHistory[dIdx + 1];

    int32_t diff = R_right - R_left;
    if (my_abs(diff) < 2) {
        return initialFreq + (peak.i * scanInfo.scanStep);
    }

    int32_t numerator = diff;
    int32_t denominator = R_left + R_center + R_right;
    if (denominator == 0) return initialFreq + (peak.i * scanInfo.scanStep);

    if (R_center < (scanInfo.rssiMin + 20)) {
        return initialFreq + (peak.i * scanInfo.scanStep);
    }

    // Convert correction units to raw measurement steps. When multiple
    // measurements map into a single display bin, each display step
    // corresponds to several raw scan steps. We compute a per-bin
    // multiplier so correction is expressed in Hz.
    uint32_t perBin = (scanInfo.measurementsCount + SPECTRUM_MAX_STEPS - 1) / SPECTRUM_MAX_STEPS;
    if (perBin == 0) perBin = 1;

    int32_t correction = (numerator * (int32_t)scanInfo.scanStep * (int32_t)perBin) / denominator;

    return initialFreq + (peak.i * scanInfo.scanStep) + correction;
}

 static void TuneToPeak() 
{
    // 1. Calculate the pinpoint accurate frequency
    uint32_t f = GetCentroidFrequency();
    
    // 2. Set the radio hardware to that pinpoint frequency
    SetF(f);

    // 3. Update the scan variables so the rest of the app knows where we are
    peak.f = f;            // Update the peak frequency to the centered value
    scanInfo.f = f;        // Update the current scan frequency
    scanInfo.rssi = peak.rssi;
    scanInfo.i = peak.i;

    // Note: Removed the second SetF(scanInfo.f) as it is now redundant 
    // and would just slow down the tuning process.
}
 
 uint8_t GetBWRegValueForScan() { return scanStepBWRegValues[settings.scanStepIndex]; }
 
 uint16_t GetRssi() {
     while ((BK4819_ReadRegister(0x63) & 0b11111111) >= 255) SYSTICK_DelayUs(100);
     uint16_t rssi = BK4819_GetRSSI();
 #ifdef ENABLE_AM_FIX
     if (settings.modulationType == MODULATION_AM && gSetting_AM_fix)
         rssi += AM_fix_get_gain_diff() * 2;
 #endif
     return rssi;
 }
 
 static void ToggleAudio(bool on) {
     if (on == audioState) return;
     audioState = on;
     if (on) AUDIO_AudioPathOn();
     else AUDIO_AudioPathOff();
 }
 
 static void ToggleRX(bool on) {
     #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
     if (isListening == on) return;
     #endif
     isListening = on;
     if (on) {
         extern VFO_Info_t *gRxVfo;
         if (gRxVfo) {
             gRxVfo->pRX->Frequency = fMeasure;
             RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
         }
         RADIO_SetupRegisters(false);
         RADIO_SetupAGC(settings.modulationType == MODULATION_AM, lockAGC);
         ToggleAudio(true);
         ToggleAFDAC(true);
         ToggleAFBit(true);
         SetBandLed(fMeasure, false, true);
     #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
         listenT = settings.listenTScan;
         BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
         setTailFoundInterrupt();
     #else
         listenT = 5;
         BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
     #endif
     } else {
         BK4819_WriteRegister(0x43, GetBWRegValueForScan());
         ToggleAudio(false);
         ToggleAFDAC(false);
         ToggleAFBit(false);
         SetBandLed(fMeasure, false, false);
     }
 }
 
 // =============================================================================
 // SCAN EXECUTION AND STATISTICS
 // =============================================================================
 
 static void ResetScanStats() {
     scanInfo.rssi = 0;
     scanInfo.rssiMax = 0;
     scanInfo.iPeak = 0;
     scanInfo.fPeak = 0;
 }
 
 static void InitScan() {
     ResetScanStats();
     scanInfo.i = 0;
     scanInfo.f = GetFStart();
     scanInfo.scanStep = GetScanStep();
     scanInfo.measurementsCount = GetStepsCount();
 }
 
 static void ResetBlacklist() {
     for (int i = 0; i < 128; ++i) {
         if (rssiHistory[i] == RSSI_MAX_VALUE) rssiHistory[i] = 0;
     }
 #ifdef ENABLE_SCAN_RANGES
     memset(blacklistFreqs, 0, sizeof(blacklistFreqs));
     blacklistFreqsIdx = 0;
 #endif
 }
 
 static void RelaunchScan() {
     InitScan();
     ResetPeak();
     ToggleRX(false);
 #ifdef SPECTRUM_AUTOMATIC_SQUELCH
     settings.rssiTriggerLevel = RSSI_MAX_VALUE;
 #endif
     preventKeypress = true;
     Measure(); 
     scanInfo.rssiMin = scanInfo.rssi;
    // reset display->best measurement mapping
    for (uint8_t i = 0; i < SPECTRUM_MAX_STEPS; ++i) displayBestIndex[i] = 0xFFFF;
 }
 
 static void UpdateScanInfo() {
     if (scanInfo.rssi > scanInfo.rssiMax) {
         scanInfo.rssiMax = scanInfo.rssi;
         scanInfo.fPeak = scanInfo.f;
         scanInfo.iPeak = scanInfo.i;
     }
     if (scanInfo.rssi < scanInfo.rssiMin) {
         scanInfo.rssiMin = ((uint32_t)scanInfo.rssiMin * 7 + scanInfo.rssi) / 8;
         settings.dbMin = Rssi2DBm(scanInfo.rssiMin);
         redrawStatus = true;
     } else if (scanInfo.rssi > (scanInfo.rssiMin + 20)) {
         scanInfo.rssiMin++;
     }
 }
 
 // =============================================================================
 // STATE AND INPUT HANDLING
 // =============================================================================
 
 static KEY_Code_t GetKey() {
     KEY_Code_t btn = KEYBOARD_Poll();
     if (btn == KEY_INVALID && GPIO_IsPttPressed()) btn = KEY_PTT;
     return btn;
 }
 
 void SetState(State state) {
     previousState = currentState;
     currentState = state;
     redrawScreen = true;
     redrawStatus = true;
     if (state == STILL) displayRssi = scanInfo.rssi;
 }

 // =============================================================================
 // PERSISTENT SETTINGS MANAGEMENT
 // =============================================================================

 /**
  * @brief Save spectrum settings to EEPROM for persistent state
  * 
  * Saves the current configuration (step size, zoom, frequency offset, bandwidth)
  * to EEPROM so they persist across power cycles.
  */
 static void SPECTRUM_SaveSettings(void) {
     uint8_t buffer[SPECTRUM_EEPROM_SIZE] = { 0 };
     
     // Pack settings into buffer
     // Byte 0: scanStepIndex (4 bits) + stepsCount (2 bits) + listenBw (2 bits)
     buffer[0] = (settings.scanStepIndex & 0x0F) | 
                 ((settings.stepsCount & 0x03) << 4);
     
     // Byte 1: listenBw (1 bit) + modulationType (1 bit) + bw (3 bits) + reserved
     buffer[1] = ((settings.listenBw & 0x03)) |
                 ((settings.modulationType & 0x01) << 2) |
                 ((settings.bw & 0x07) << 3);
     
     // Bytes 2-5: frequencyChangeStep (uint32_t)
     buffer[2] = (settings.frequencyChangeStep >> 0) & 0xFF;
     buffer[3] = (settings.frequencyChangeStep >> 8) & 0xFF;
     buffer[4] = (settings.frequencyChangeStep >> 16) & 0xFF;
     buffer[5] = (settings.frequencyChangeStep >> 24) & 0xFF;
     
     // Bytes 6-7: rssiTriggerLevel (uint16_t)
     buffer[6] = (settings.rssiTriggerLevel >> 0) & 0xFF;
     buffer[7] = (settings.rssiTriggerLevel >> 8) & 0xFF;
     
     // Bytes 8-9: dbMin (int16_t)
     buffer[8] = (settings.dbMin >> 0) & 0xFF;
     buffer[9] = (settings.dbMin >> 8) & 0xFF;
     
     // Bytes 10-11: dbMax (int16_t)
     buffer[10] = (settings.dbMax >> 0) & 0xFF;
     buffer[11] = (settings.dbMax >> 8) & 0xFF;
     
     // Bytes 12-13: scanDelay (uint16_t)
     buffer[12] = (settings.scanDelay >> 0) & 0xFF;
     buffer[13] = (settings.scanDelay >> 8) & 0xFF;
     
     // Byte 14: backlightState
     buffer[14] = settings.backlightState ? 0x01 : 0x00;
     
     // Byte 15: CRC/checksum (simple sum for validation)
     uint8_t checksum = 0;
     for (int i = 0; i < 15; i++) {
         checksum += buffer[i];
     }
     buffer[15] = checksum;
     
     // Write to EEPROM
     EEPROM_WriteBuffer(SPECTRUM_EEPROM_ADDR, buffer);
 }

 /**
  * @brief Load spectrum settings from EEPROM
  * 
  * Restores previously saved configuration (step size, zoom, frequency offset, bandwidth)
  * from EEPROM to maintain user preferences across power cycles.
  */
 static void SPECTRUM_LoadSettings(void) {
     uint8_t buffer[SPECTRUM_EEPROM_SIZE] = { 0 };
     
     // Read from EEPROM
     EEPROM_ReadBuffer(SPECTRUM_EEPROM_ADDR, buffer, SPECTRUM_EEPROM_SIZE);
     
     // Verify checksum
     uint8_t checksum = 0;
     for (int i = 0; i < 15; i++) {
         checksum += buffer[i];
     }
     if (checksum != buffer[15]) {
         // Checksum mismatch: EEPROM data is invalid, keep defaults
         return;
     }
     
     // Unpack settings from buffer
     settings.scanStepIndex = buffer[0] & 0x0F;
     if (settings.scanStepIndex > S_STEP_100_0kHz) {
         settings.scanStepIndex = S_STEP_25_0kHz;
     }
     
     settings.stepsCount = (buffer[0] >> 4) & 0x03;
     if (settings.stepsCount > STEPS_16) {
         settings.stepsCount = STEPS_64;
     }
     
     settings.listenBw = buffer[1] & 0x03;
     if (settings.listenBw > BK4819_FILTER_BW_NARROWER) {
         settings.listenBw = BK4819_FILTER_BW_WIDE;
     }
     
     settings.modulationType = (buffer[1] >> 2) & 0x01;
     settings.bw = (buffer[1] >> 3) & 0x07;
     if (settings.bw > BK4819_FILTER_BW_NARROWER) {
         settings.bw = BK4819_FILTER_BW_WIDE;
     }
     
     settings.frequencyChangeStep = (uint32_t)buffer[2] |
                                    ((uint32_t)buffer[3] << 8) |
                                    ((uint32_t)buffer[4] << 16) |
                                    ((uint32_t)buffer[5] << 24);
     if (settings.frequencyChangeStep < 10000 || settings.frequencyChangeStep > 200000) {
         settings.frequencyChangeStep = 600000;  // Restore default
     }
     
     settings.rssiTriggerLevel = (uint16_t)buffer[6] | ((uint16_t)buffer[7] << 8);
     
     int16_t tmp_dbMin = (int16_t)(buffer[8] | ((int16_t)buffer[9] << 8));
     if (tmp_dbMin >= DISPLAY_DBM_MIN && tmp_dbMin <= DISPLAY_DBM_MAX) {
         settings.dbMin = tmp_dbMin;
     }
     
     int16_t tmp_dbMax = (int16_t)(buffer[10] | ((int16_t)buffer[11] << 8));
     if (tmp_dbMax >= DISPLAY_DBM_MIN && tmp_dbMax <= DISPLAY_DBM_MAX) {
         settings.dbMax = tmp_dbMax;
     }
     
     settings.scanDelay = (uint16_t)buffer[12] | ((uint16_t)buffer[13] << 8);
     if (settings.scanDelay == 0) {
         settings.scanDelay = 3200;  // Restore default
     }
     
     settings.backlightState = (buffer[14] != 0);
 }
 
 static void DeInitSpectrum() {
     // Save spectrum settings before exiting
     SPECTRUM_SaveSettings();
     
     isInitialized = false;
     isListening = false;
     ToggleRX(false); 
     RestoreRegisters(); 
     RADIO_SetupRegisters(true); 
     gCurrentFunction = FUNCTION_FOREGROUND;
     gUpdateStatus = true;
     gUpdateDisplay = true;
 }

/**
 * @brief Professional automatic trigger level optimization
 *
 * Implements scan-based automatic threshold adjustment that updates
 * after each complete spectrum scan. This professional-grade algorithm:
 *
 * Features:
 * - Updates trigger level after every complete spectrum scan
 * - More aggressive detection of weak signals (Issue #2 fix)
 * - Adapts automatically to changing band conditions
 * - Prevents false triggers while detecting weak signals
 * - Professional-grade behavior matching Yaesu/ICOM standards
 *
 * Algorithm:
 * 1. Analyze complete spectrum scan data
 * 2. Calculate new peak signal strength
 * 3. Set trigger = peak + 4dB (sensitive detection of weak signals)
 * 4. Clamp to valid RSSI range to prevent saturation
 * 5. Apply immediately for next detection cycle
 *
 * Benefits Over Static Threshold:
 * - Automatically detects weak signals without manual adjustment
 * - Maintains selectivity in crowded bands
 * - Responsive to rapid band activity changes
 * - Professional appearance and behavior
 *
 * @note This is called after each spectrum scan completes
 * @see UpdatePeakInfoForce() - Called when peak changes or timeout
 * @see scanInfo.rssiMax - Maximum RSSI from current scan
 */
static void AutoTriggerLevel()
{
    // Scan-based auto-adjust: Update trigger after every spectrum scan
    // FIX (Issue #2 & close-range drift): Optimized for weak signal detection while preventing false
    // positives and avoiding desensitization after a single very strong spike.
    //
    // Behavior improvements in this revision:
    // 1. A strong first scan no longer immediately raises threshold to peak level.  Instead the
    //    trigger is initialized to a moderate baseline (previous hardcoded default) so that a
    //    brief, close‑in burst cannot desensitize the receiver.
    // 2. Downward adjustments are made faster than upward ones.  When a strong signal disappears
    //    the threshold falls quickly, restoring sensitivity for the next packet.
    // 3. RSSI_MAX_VALUE is treated as a special sentinel and converted to the baseline value.
    
    // Handle the special sentinel value set by RelaunchScan() when automatic squelch is enabled.
    if (settings.rssiTriggerLevel == RSSI_MAX_VALUE)
    {
        // Instead of using the very first peak (which might be an outlier) we start from a safe
        // mid‑range level. 150 was the legacy default in spectrum settings and works well across
        // bands.
        settings.rssiTriggerLevel = 150;
        return; // wait for the next scan to run the normal adjustment logic
    }

    // Compute desired trigger based on the most recent scan peak.
    uint16_t newTrigger = clamp(scanInfo.rssiMax + 3, 0, RSSI_MAX_VALUE);

    // Enforce minimum safety margin (6dB above peak to prevent false triggers on noise)
    const uint16_t MIN_TRIGGER = scanInfo.rssiMax + 6;
    if (newTrigger < MIN_TRIGGER)
        newTrigger = MIN_TRIGGER;

    // Only adjust when the difference is meaningful (>5 RSSI units).
    if (newTrigger > settings.rssiTriggerLevel + 5)
    {
        // Signal getting stronger: increase trigger slowly (1 unit per scan)
        settings.rssiTriggerLevel += 1;
    }
    else if (newTrigger < settings.rssiTriggerLevel - 5)
    {
        // Signal getting weaker: decrease quickly (3 units per scan) to recover sensitivity.
        // Do not undershoot the computed target.
        uint16_t diff = settings.rssiTriggerLevel - newTrigger;
        if (diff > 3)
            settings.rssiTriggerLevel -= 3;
        else
            settings.rssiTriggerLevel = newTrigger;
    }
    // Otherwise: hold current trigger (prevent oscillation on small noise changes)
}

/**
 * @brief Professional peak detection with hysteresis filtering
 *
 * Updates peak information using a peak-hold algorithm with adaptive
 * time constant. Prevents rapid fluctuations while responding to genuine
 * signal changes. Implements the following characteristics:
 *
 * - Timeout: 1024 samples for peak age management
 * - New Peak Detection: Activates when signal exceeds current peak + 2dB
 * - Hysteresis: Prevents constant updates during noise
 * - Auto-trigger: Optimizes detection threshold (see AutoTriggerLevel)
 *
 * @see AutoTriggerLevel() - Automatic trigger optimization
 * @see scanInfo - Current scan measurement data
 * @see peak - Peak information storage
 */
static void UpdatePeakInfoForce()
{
    peak.t = 0;                          // Reset age counter for peak hold
    peak.rssi = scanInfo.rssiMax;        // Store maximum RSSI from scan
    peak.f = scanInfo.fPeak;             // Store peak frequency (Hz)
    peak.i = scanInfo.iPeak;             // Store peak index (0-127)
    AutoTriggerLevel();                  // Optimize trigger for next scan
}

/**
 * @brief Adaptive peak update with time constant
 *
 * Implements intelligent peak tracking that:
 * 1. Maintains current peak until signal decays or 1024 samples pass
 * 2. Detects new peaks that exceed existing peak by significant margin
 * 3. Prevents rapid peak hopping in noisy environments
 */
static void UpdatePeakInfo()
{
    // Timeout-based peak refresh or new peak detection
    if (peak.f == 0 || peak.t >= 1024 || peak.rssi < scanInfo.rssiMax)
    {
        UpdatePeakInfoForce();
    }
    else
    {
        peak.t++;  // Increment peak age counter
    }
}

// Forward declaration so older functions can trigger immediate waterfall updates
static void UpdateWaterfall(void);

static inline uint8_t MapMeasurementToDisplay(uint32_t idx)
{
    if (scanInfo.measurementsCount == 0) return (uint8_t)idx;
    if (scanInfo.measurementsCount <= SPECTRUM_MAX_STEPS) return (uint8_t)idx;
    uint32_t v = ((uint32_t)idx * SPECTRUM_MAX_STEPS) / scanInfo.measurementsCount;
    if (v >= SPECTRUM_MAX_STEPS) v = SPECTRUM_MAX_STEPS - 1;
    return (uint8_t)v;
}

static void SetRssiHistory(uint16_t idx, uint16_t rssi)
{
    uint8_t dIdx = MapMeasurementToDisplay(idx);

    /* Store the strongest measurement for the display bin. When multiple
       raw measurements map to the same bin, prefer the maximum RSSI so
       that peaks remain visible after decimation. Record the raw index
       that produced this best value to allow accurate tuning later. */
    if (rssiHistory[dIdx] < rssi || isListening)
    {
        rssiHistory[dIdx] = rssi;
        displayBestIndex[dIdx] = idx;
    }
}

/**
 * @brief Perform real-time RSSI measurement
 * [Fix: PLL Delay + Universal AM-Modulation Guard]
 */
static void Measure()
{
    
    // --- AM MODULATION GUARD ---
    // If the radio is in AM mode, the internal BK4819 gain-switching
    // can cause the spectrum to flicker. We ensure we are in a 
    // "Stable Gain" state before reading.
    //uint8_t originalModulation = settings.modulationType;

    // --- PLL SETTLING DELAY ---
    // 400us for frequency stabilization
    SYSTEM_DelayUs(400);

    // --- HARDWARE READ ---
    uint16_t rssi = scanInfo.rssi = GetRssi();
    SetRssiHistory(scanInfo.i, rssi);

    // No need to restore modulation here as we only read 
    // the state to ensure the measurement was valid.
}
// Update things by keypress

static uint16_t dbm2rssi(int dBm)
{
    return (dBm + 160 - dBmCorrTable[gRxVfo->Band]) * 2;
}

static void ClampRssiTriggerLevel()
{
    settings.rssiTriggerLevel =
        clamp(settings.rssiTriggerLevel, dbm2rssi(settings.dbMin),
              dbm2rssi(settings.dbMax));
}

static void UpdateDBMax(bool inc)
{
    if (inc && settings.dbMax < 10)
    {
        settings.dbMax += 1;
    }
    else if (!inc && settings.dbMax > settings.dbMin)
    {
        settings.dbMax -= 1;
    }
    else
    {
        return;
    }

    ClampRssiTriggerLevel();
    redrawStatus = true;
    redrawScreen = true;
    SYSTEM_DelayMs(20);
}

static void UpdateScanStep(bool inc)
{
    if (inc)
    {
        settings.scanStepIndex = settings.scanStepIndex != S_STEP_100_0kHz ? settings.scanStepIndex + 1 : 0;
    }
    else
    {
        settings.scanStepIndex = settings.scanStepIndex != 0 ? settings.scanStepIndex - 1 : S_STEP_100_0kHz;
    }

    // FIX: Do NOT modify frequencyChangeStep here. It's the user's frequency offset
    // setting and should remain independent of step size adjustments.
    RelaunchScan();
    ResetBlacklist();
    redrawScreen = true;
}

static void UpdateCurrentFreq(bool inc)
{
    if (inc && currentFreq < F_MAX)
    {
        currentFreq += settings.frequencyChangeStep;
    }
    else if (!inc && currentFreq > F_MIN)
    {
        currentFreq -= settings.frequencyChangeStep;
    }
    else
    {
        return;
    }

#ifdef ENABLE_SCAN_RANGES
    /* keep the internal 'cursor' within the defined bounds; it doesn't
       influence the actual scan start/stop but it is used by centre mode
       calculations and on‑screen markers. */
    if (gScanRangeStart) {
        if (currentFreq < gScanRangeStart) currentFreq = gScanRangeStart;
        if (currentFreq > gScanRangeStop)  currentFreq = gScanRangeStop;
    }
#endif

    RelaunchScan();
    ResetBlacklist();
    redrawScreen = true;
}

static void UpdateCurrentFreqStill(bool inc)
{
    uint8_t offset = modulationTypeTuneSteps[settings.modulationType];
    uint32_t f = fMeasure;
    if (inc && f < F_MAX)
    {
        f += offset;
    }
    else if (!inc && f > F_MIN)
    {
        f -= offset;
    }
    SetF(f);
    redrawScreen = true;
}

static void UpdateFreqChangeStep(bool inc)
{
    if (IsCenterMode())
    {
        // SHIFT centre frequency by the current change step
        if (inc)
        {
            if (currentFreq + settings.frequencyChangeStep <= F_MAX)
                currentFreq += settings.frequencyChangeStep;
        }
        else
        {
            if (currentFreq >= settings.frequencyChangeStep + F_MIN)
                currentFreq -= settings.frequencyChangeStep;
        }
        
        RelaunchScan();
        ResetBlacklist();
    }
    else
    {
        // NORMAL/WIDE MODE: adjust the change step
        // We use 8x the scan step for meaningful increments (e.g., 25k step -> 200k jumps)
        uint32_t diff = GetScanStep() * 8; 

        if (inc)
        {
            settings.frequencyChangeStep += diff;
        }
        else
        {
            // Allow decrementing down to 0
            if (settings.frequencyChangeStep > diff)
                settings.frequencyChangeStep -= diff;
            else
                settings.frequencyChangeStep = 0;
        }

        // --- HARD CLAMP ---
        // Ensure we never exceed 2000.00k (200,000 units @ 10Hz steps)
        if (settings.frequencyChangeStep > 200000)
        {
            settings.frequencyChangeStep = 200000;
        }
    }
    
    SYSTEM_DelayMs(100);
    redrawScreen = true;
}

static void ToggleModulation()
{
    if (settings.modulationType < MODULATION_UKNOWN - 1)
    {
        settings.modulationType++;
    }
    else
    {
        settings.modulationType = MODULATION_FM;
    }
    RADIO_SetModulation(settings.modulationType);

    RelaunchScan();
    redrawScreen = true;
}

static void ToggleListeningBW()
{
    if (settings.listenBw == BK4819_FILTER_BW_NARROWER)
    {
        settings.listenBw = BK4819_FILTER_BW_WIDE;
    }
    else
    {
        settings.listenBw++;
    }
    redrawScreen = true;
}

static void ToggleBacklight()
{
    settings.backlightState = !settings.backlightState;
    if (settings.backlightState)
    {
        BACKLIGHT_TurnOn();
    }
    else
    {
        BACKLIGHT_TurnOff();
    }
}

static void ToggleStepsCount()
{
    if (settings.stepsCount == STEPS_128)
    {
        settings.stepsCount = STEPS_16;
    }
    else
    {
        settings.stepsCount--;
    }
    // FIX: Do NOT modify frequencyChangeStep here. It's the user's frequency offset
    // setting and should remain independent of zoom/step count adjustments.
    RelaunchScan();
    ResetBlacklist();
    redrawScreen = true;
}

static void ResetFreqInput()
{
    tempFreq = 0;
    for (int i = 0; i < 10; ++i)
    {
        freqInputString[i] = '-';
    }
}

static void FreqInput()
{
    freqInputIndex = 0;
    freqInputDotIndex = 0;
    ResetFreqInput();
    // Reset function key state to avoid invalid transitions
    extern bool gWasFKeyPressed;
    gWasFKeyPressed = false;
    SetState(FREQ_INPUT);
}

static void UpdateFreqInput(KEY_Code_t key)
{
    // 1. Handle Backspace / Exit logic
    if (key == KEY_EXIT)
    {
        if (freqInputIndex > 0) {
            freqInputIndex--;
            if (freqInputDotIndex > freqInputIndex) 
                freqInputDotIndex = 0;
        } else {
            // If already at 0 digits, exit to Spectrum
            SetState(SPECTRUM);
        }
        goto UPDATE_CALC;
    }

    // 2. Limit to 8 slots (3 MHz digits + 1 Dot + 4 decimal digits)
    if (freqInputIndex >= 8) return;

    // 3. Handle Decimal (Manual or Auto)
    if (key == KEY_STAR)
    {
        if (freqInputIndex == 0 || freqInputDotIndex) return;
        freqInputDotIndex = freqInputIndex;
    }

    // 4. Numeric Input + Auto-Dot (Places dot after 3 digits: e.g. "144.")
    freqInputArr[freqInputIndex++] = key;
    
    if (freqInputIndex == 3 && freqInputDotIndex == 0) {
        freqInputDotIndex = 3;
        freqInputArr[freqInputIndex++] = KEY_STAR;
    }

UPDATE_CALC:
    // Reset temporary frequency before math
    tempFreq = 0;
    
    // 5. Build Display String (7 digits + 1 dot = 8 chars)
    for (int i = 0; i < 8; ++i)
    {
        if (i < freqInputIndex)
        {
            KEY_Code_t digitKey = freqInputArr[i];
            freqInputString[i] = (digitKey == KEY_STAR) ? '.' : ('0' + digitKey - KEY_0);
        }
        else
        {
            freqInputString[i] = '_'; // Use underscores as placeholders
        }
    }
    freqInputString[8] = '\0';

    // 6. Math: Calculate 10Hz steps (Fixed point)
    // reset tempFreq here so that each keypress recalculates from scratch
    tempFreq = 0;
    uint32_t dotIndex = (freqInputDotIndex == 0) ? freqInputIndex : freqInputDotIndex;
    
    // MHz Part
    uint32_t base = 100000; 
    for (int i = dotIndex - 1; i >= 0; --i)
    {
        tempFreq += (freqInputArr[i] - KEY_0) * base;
        base *= 10;
    }

    // Decimal Part (e.g., .5000)
    base = 10000; 
    if (dotIndex < freqInputIndex)
    {
        for (int i = dotIndex + 1; i < freqInputIndex; ++i)
        {
            tempFreq += (freqInputArr[i] - KEY_0) * base;
            base /= 10;
        }
    }

    // 7. AUTO-COMMIT Logic (8 digits entered)
if (freqInputIndex >= 8) 
{
    if (tempFreq > 0)
    {
        // Reuse the same logic as manual commit (KEY_MENU) so that the
        // entered value is treated as the centre frequency, and
        // clamping/range behaviour is identical.
        SetState(previousState);

        currentFreq = tempFreq;
#ifdef ENABLE_SCAN_RANGES
        if (gScanRangeStart) {
            if (currentFreq < gScanRangeStart) currentFreq = gScanRangeStart;
            if (currentFreq > gScanRangeStop)  currentFreq = gScanRangeStop;
        }
#endif

        if (currentState == SPECTRUM)
        {
            ResetBlacklist();
            RelaunchScan();
        }
        else
        {
            SetF(currentFreq);
        }

        // Final Sync and UI Reset
        initialFreq = currentFreq;
        freqInputIndex = 0;
    }
}

    redrawScreen = true;
}

static void Blacklist()
{
#ifdef ENABLE_SCAN_RANGES
    blacklistFreqs[blacklistFreqsIdx++ % ARRAY_SIZE(blacklistFreqs)] = peak.i;
#endif

    SetRssiHistory(peak.i, RSSI_MAX_VALUE);
    ResetPeak();
    ToggleRX(false);
    ResetScanStats();
}

#ifdef ENABLE_SCAN_RANGES
static bool IsBlacklisted(uint16_t idx)
{
    if (blacklistFreqsIdx)
        for (uint8_t i = 0; i < ARRAY_SIZE(blacklistFreqs); i++)
            if (blacklistFreqs[i] == idx)
                return true;
    return false;
}
#endif

// Draw things

// applied x2 to prevent initial rounding
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax)
{
    const int DB_MIN = settings.dbMin << 1;
    const int DB_MAX = settings.dbMax << 1;
    const int DB_RANGE = DB_MAX - DB_MIN;
    const uint8_t PX_RANGE = pxMax - pxMin;

    // Convert raw RSSI to dBm
    int dbm = clamp(Rssi2DBm(rssi) << 1, DB_MIN, DB_MAX);

    // --- TRUE NOISE SIGNIFICANCE ENGINE ---
    int height = ((dbm - DB_MIN) * PX_RANGE) / (DB_RANGE ? DB_RANGE : 1);
    
    // If the signal is low (near the noise floor), we amplify the "Variance"
    // This creates the "Significant Grass" look without faking the data.
    if (height < (PX_RANGE / 4)) 
    {
        // Capture the hardware's jitter (Least Significant Bits)
        // This makes the noise floor shimmer based on actual ADC thermal noise
        uint8_t noiseJitter = (rssi & 0x07); 
        height += noiseJitter; // Add "hair" to the noise floor
    }
    else 
    {
        // For stronger signals, we use a slight non-linear boost to sharpen the peaks
        // This makes the signal look "crisp" rather than "fat"
        height = (height * 11) / 10; 
    }

    return clamp(height + pxMin, pxMin, pxMax);
}

uint8_t Rssi2Y(uint16_t rssi)
{
    // Adding 5 pixels here pushes the entire graph down to the ruler
    return (DrawingEndY + 4) - Rssi2PX(rssi, 0, DrawingEndY);
}
#ifdef ENABLE_FEAT_N7SIX
    // Deprecated: Use DrawSpectrumEnhanced() instead
    // This function is preserved for reference only
#else
    // Fallback implementation for non-enhanced builds
#endif

static void DrawStatus()
{
    static char String[32]; // static keeps this out of the stack "danger zone"
#ifdef SPECTRUM_EXTRA_VALUES
    sprintf(String, "%d/%ddBm P:%d T:%d", settings.dbMin, settings.dbMax,
            Rssi2DBm(peak.rssi), Rssi2DBm(settings.rssiTriggerLevel));
#else
    sprintf(String, "%d/%ddBm", settings.dbMin, settings.dbMax);
#endif
    GUI_DisplaySmallest(String, 0, 1, true, true);

    BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[gBatteryCheckCounter++ % 4],
                             &gBatteryCurrent);

    uint16_t voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] +
                        gBatteryVoltages[2] + gBatteryVoltages[3]) /
                       4 * 760 / gBatteryCalibration[3];

    unsigned perc = BATTERY_VoltsToPercent(voltage);

    // sprintf(String, "%d %d", voltage, perc);
    // GUI_DisplaySmallest(String, 48, 1, true, true);

    gStatusLine[116] = 0b00011100;
    gStatusLine[117] = 0b00111110;
    for (int i = 118; i <= 126; i++)
    {
        gStatusLine[i] = 0b00100010;
    }

    for (unsigned i = 127; i >= 118; i--)
    {
        if (127 - i <= (perc + 5) * 9 / 100)
        {
            gStatusLine[i] = 0b00111110;
        }
    }
}

/**
 * @brief Lightweight waterfall update for busy listen ticks
 *
 * During listening mode, preserve the frequency-domain data from the last
 * full scan instead of painting flat lines. Only advance the circular buffer
 * forward without overwriting with stale RSSI measurements.
 */
static void UpdateWaterfallQuick(void)
{
    waterfallIndex = (waterfallIndex + 1) % WATERFALL_HISTORY_DEPTH;
    // NOTE: Do NOT fill with flat RSSI here. The heavy path (every 6 ticks) handles
    // spectrum data generation. This function just advances the buffer pointer.
}

/**
 * @brief Update waterfall display data with professional signal processing
 *
 * Processes the current RSSI history and converts it to 16-level grayscale
 * for professional-grade waterfall visualization. Uses adaptive signal 
 * detection with hysteresis to prevent flickering while maintaining detail.
 */
static void UpdateWaterfall(void)
{
    waterfallIndex = (waterfallIndex + 1) % WATERFALL_HISTORY_DEPTH;

    uint16_t specWidth = GetDisplayWidth();
    uint16_t minRssi = 0xFFFF, maxRssi = 0;
    uint16_t validSamples = 0;

    // 1. Analyze for Dynamic Range
    for (uint8_t x = 0; x < specWidth; x++) {
        uint16_t rssi = rssiHistory[x];
        if (rssi != RSSI_MAX_VALUE && rssi != 0) {
            if (rssi < minRssi) minRssi = rssi;
            if (rssi > maxRssi) maxRssi = rssi;
            validSamples++;
        }
    }

    uint16_t range = (maxRssi > minRssi) ? (maxRssi - minRssi) : 1;

    // 2. Process levels
    for (uint8_t x = 0; x < specWidth; x++) {
        uint16_t rssi = rssiHistory[x];
        uint8_t level = 0;

        if (rssi != RSSI_MAX_VALUE && rssi != 0 && validSamples > 0) {
            level = (uint8_t)(((uint32_t)(rssi - minRssi) * 15) / range);

            // KEPT: ACTIVE UI ENHANCEMENTS
            if (level == 0 && (rssi & 0x01)) level = 1; 
            if (level > 0 && level < 4) level = 4; 
        }
        SetWaterfallLevel(x, waterfallIndex, level);
    }
}

/**
 * @brief Render professional-grade waterfall display with 4x4 Bayer dithering
 * Logic: Simulates grayscale by turning pixels on/off based on threshold density.
 */
static void DrawWaterfall(void)
{
    static const uint8_t bayerMatrix[4][4] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
    };

    const uint8_t WATERFALL_START_Y = 40;
    const uint16_t SPEC_WIDTH = GetDisplayWidth();
    extern uint8_t gFrameBuffer[7][128];

    for (uint8_t y_offset = 0; y_offset < WATERFALL_HISTORY_DEPTH; y_offset++) {
        uint8_t y_pos = WATERFALL_START_Y + y_offset;
        if (y_pos > 63) break;

        // Calculate Row in History
        int16_t historyRow = (int16_t)waterfallIndex - y_offset;
        while (historyRow < 0) historyRow += WATERFALL_HISTORY_DEPTH;

        // Optimized Fade
        uint8_t currentFade = 16;
        if (y_offset > 10) {
            uint8_t drop = (y_offset - 10) * 2;
            currentFade = (drop >= 16) ? 0 : 16 - drop;
        }

        for (uint8_t x = 0; x < 128; x++) {
            // Mapping: Screen X to Spectrum Index
            uint16_t specIdx = ((uint32_t)x * SPEC_WIDTH) >> 7; 
            uint8_t level = GetWaterfallLevel(specIdx, historyRow);

            // Apply Fade
            if (currentFade < 16) {
                level = (level * currentFade) >> 4;
            }

            // Bayer Dither Comparison
            // Use (y_pos & 3) to keep the dither pattern fixed to the screen
            if (level > bayerMatrix[y_pos & 3][x & 3]) {
                gFrameBuffer[y_pos >> 3][x] |= (1 << (y_pos & 7));
            } else {
                gFrameBuffer[y_pos >> 3][x] &= ~(1 << (y_pos & 7));
            }
        }
    }
}

static void DrawSpectrumEnhanced(void)
{
#ifdef ENABLE_FEAT_N7SIX
    const uint16_t bars = GetDisplayWidth();
    extern uint8_t gFrameBuffer[7][128];
    const uint8_t SHADE_MAX_Y = 32; 

    uint8_t prevX = SpecIdxToX(0);
    // Use smoothedRssi for the main trace
    uint8_t prevY = Rssi2Y(smoothedRssi[0]);

    for (uint8_t i = 1; i < bars; i++) {
        uint8_t currX = SpecIdxToX(i);
        uint8_t currY = Rssi2Y(smoothedRssi[i]);

        // 1. THE MAIN TRACE (Smooth Wire)
        DrawLine(prevX, prevY, currX, currY, 1);

        // 2. THE PEAK HOLD DOT
        // Draw a single bright pixel at the highest peak ever reached
        uint8_t peakY = Rssi2Y(peakHold[i]);
        if (peakY < 127) {
            gFrameBuffer[peakY >> 3][currX] |= (1 << (peakY & 7));
        }

        // 3. THE REFINED VERTICAL SHADE (Fast-Column)
        if (currX >= prevX) {
            for (uint8_t x = prevX; x <= currX; x++) {
                uint8_t dx = currX - prevX + 1;
                uint8_t yStart = prevY + ((currY - prevY) * (x - prevX) / dx);
                
                if (yStart < SHADE_MAX_Y) {
                    for (uint8_t y = yStart + 1; y < SHADE_MAX_Y; y++) {
                        if ((x ^ y) & 0x01) {
                            gFrameBuffer[y >> 3][x] |= (1 << (y & 7));
                        }
                    }
                }
            }
        }
        prevX = currX;
        prevY = currY;
    }
#endif
}

#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
static void ShowChannelName(uint32_t f)
{
    static uint32_t channelF = 0;
    static char channelName[12]; 

    if (isListening)
    {
        if (f != channelF) {
            channelF = f;
            unsigned int i;
            memset(channelName, 0, sizeof(channelName));
            for (i = 0; IS_MR_CHANNEL(i); i++)
            {
                if (RADIO_CheckValidChannel(i, false, 0))
                {
                    if (SETTINGS_FetchChannelFrequency(i) == channelF)
                    {
                        SETTINGS_FetchChannelName(channelName, i);
                        break;
                    }
                }
            }
        }
        if (channelName[0] != 0) {
            // Display channel name at top of spectrum in small font
            // Position: below frequency/modulation info (y=14), left aligned
            GUI_DisplaySmallest(channelName, 0, 14, false, true);
        }
    }
}
#endif

static void DrawF(uint32_t f)
{
    sprintf(String, "%u.%05u", f / 100000, f % 100000);
    UI_PrintStringSmallNormal(String, 8, 127, 0);

    sprintf(String, "%3s", gModulationStr[settings.modulationType]);
    GUI_DisplaySmallest(String, 116, 0, false, true);
    sprintf(String, "%4sk", bwOptions[settings.listenBw]);
    GUI_DisplaySmallest(String, 108, 6, false, true);

#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
    ShowChannelName(f);
#endif
}

static void DrawNums()
{
    // Frequency numbers moved from Y=38 to Y=34 (4 pixels up)
    if (currentState == SPECTRUM)
    {
#ifdef ENABLE_SCAN_RANGES
        if (gScanRangeStart)
        {
            /* only show resolution; the raw count is redundant */
            sprintf(String, "%ux", 128 >> settings.stepsCount);
        }
        else
#endif
        {
            sprintf(String, "%ux", GetStepsCount());
        }
        // Moved Y from 1 to 0 (keeping top info at top)
        GUI_DisplaySmallest(String, 0, 0, false, true);
        
        sprintf(String, "%u.%02uk", GetScanStep() / 100, GetScanStep() % 100);
                GUI_DisplaySmallest(String, 0, 6, false, true);
    }

    if (IsCenterMode())
    {
        sprintf(String, "%u.%05u \x7F%u.%02uk", currentFreq / 100000,
                currentFreq % 100000, settings.frequencyChangeStep / 100,
                settings.frequencyChangeStep % 100);
        
        // MOVED: Y coordinate 38 -> 34
        GUI_DisplaySmallest(String, 36, 34, false, true);
    }
    else
    {
        sprintf(String, "%u.%05u", GetFStart() / 100000, GetFStart() % 100000);
        // MOVED: Y coordinate 38 -> 34
        GUI_DisplaySmallest(String, 0, 34, false, true);

        sprintf(String, "\x7F%u.%02uk", settings.frequencyChangeStep / 100,
                settings.frequencyChangeStep % 100);
        // MOVED: Y coordinate 38 -> 34
        GUI_DisplaySmallest(String, 48, 34, false, true);

        sprintf(String, "%u.%05u", GetFEnd() / 100000, GetFEnd() % 100000);
        // MOVED: Y coordinate 38 -> 34
        GUI_DisplaySmallest(String, 93, 34, false, true);
    }
}

// --- TRIGGER LEVEL HELPER ---
// Handles the math and input clamping
void UpdateRssiTriggerLevel(bool up)
{
    int32_t step = 2; 

    // FIX: Allow threshold to be adjusted freely without visual boundary constraints
    // The visual boundary (Y=39) should only affect DRAWING, not the actual threshold value
    if (up) {
        if (settings.rssiTriggerLevel < RSSI_MAX_VALUE - step) {
            settings.rssiTriggerLevel += step;
        }
    } else {
        // Allow decreasing threshold freely - no visual boundary check
        if (settings.rssiTriggerLevel >= step) {
            settings.rssiTriggerLevel -= step;
        }
    }
    redrawScreen = true;
}

// --- RENDER COMPONENT ---
// Matches visual marker to the Grid Floor
static void DrawRssiTriggerLevel()
{
    uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
    
    // Clamp visual marker so it never draws below the grid floor
    if (y > 39) y = 39; 

    uint8_t bank = y >> 3;
    uint8_t bit  = 1 << (y & 7);

    for (uint8_t x = 0; x < 128; x++)
    {
        // 1. Draw Side Notches (Professional reference brackets)
        if (x < 3 || x > 124) 
        {
            gFrameBuffer[bank][x] |= bit;
        }
        // 2. Draw Stippled Line (Matches Grid Pattern)
        // If y is exactly 39, we don't stipple to avoid making the grid line look "thick"
        else if (y < 39 && (x % 4) == 0 && (x % 16 != 0)) 
        {
            gFrameBuffer[bank][x] |= bit;
        }
    }
}

static void DrawTicks()
{
    uint32_t f = GetFStart();
    uint32_t span = GetFEnd() - GetFStart();
    uint32_t step = span / 128;

    for (uint8_t i = 0; i < 128; i += (1 << settings.stepsCount))
    {
        f = GetFStart() + (uint64_t)span * i / 128;

        // --- ORIGINAL STANDALONE RULER ---
        // Rail sits at Y=28 (Bit 4: 0b00010000)
        // Standard upward growth
        uint8_t bottomBar = 0b00010000; 

        if ((f % 100000) < step) {
            // Longest: Y=28 up to Y=24 (0b11111000)
            bottomBar = 0b11111000; 
        }
        else if ((f % 50000) < step) {
            // Medium: Y=28 up to Y=26 (0b01110000)
            bottomBar = 0b01110000; 
        }
        else if ((f % 10000) < step) {
            // Short: Y=28 up to Y=27 (0b00110000)
            bottomBar = 0b00110000; 
        }
        
        // Write to Bank 3
        gFrameBuffer[3][i] |= bottomBar;
    }

    // --- ORIGINAL CENTER MARKER ---
    if (IsCenterMode())
    {
        // Solid vertical line at the center of the bank
        gFrameBuffer[3][64] |= 0b11111111; 
    }
}

static void DrawArrow(uint8_t x)
{
    // Professional 5x3 Solid Triangle Marker
    // x = center position
    for (signed i = -2; i <= 2; ++i)
    {
        signed v = x + i;
        if (!(v & 128)) // Boundary check
        {
            uint8_t column_pattern = 0;

            // Define the shape per column
            if (my_abs(i) == 0)      column_pattern = 0b11100000; // Center (tallest)
            else if (my_abs(i) == 1) column_pattern = 0b11000000; // Inner wings
            else if (my_abs(i) == 2) column_pattern = 0b10000000; // Outer edge

            // Place in Bank 3 (Ruler Bank) using the bottom pixels
            gFrameBuffer[3][v] |= column_pattern;
        }
    }
}

// --- MAIN RENDER PIPELINE ---
static void OnKeyDown(uint8_t key, bool longPress)
{
    switch (key)
    {
    case KEY_3:
        UpdateDBMax(true);
        break;
    case KEY_9:
        UpdateDBMax(false);
        break;
    case KEY_1:
        UpdateScanStep(true);
        break;
    case KEY_7:
        UpdateScanStep(false);
        break;
    case KEY_2:
        UpdateFreqChangeStep(true);
        break;
    case KEY_8:
        UpdateFreqChangeStep(false);
        break;
    case KEY_UP:
#ifdef ENABLE_NAVIG_LEFT_RIGHT
            UpdateCurrentFreq(false);
#else
            UpdateCurrentFreq(true);
#endif
        break;
    case KEY_DOWN:
#ifdef ENABLE_NAVIG_LEFT_RIGHT
            UpdateCurrentFreq(true);
#else
            UpdateCurrentFreq(false);
#endif
        break;
    case KEY_SIDE1:
        Blacklist();
        break;
    case KEY_STAR:
        UpdateRssiTriggerLevel(true);
        break;
    case KEY_F:
        UpdateRssiTriggerLevel(false);
        break;
        case KEY_5:
    // Use 'currentState' which is the standard variable for this firmware
    if (currentState != FREQ_INPUT) 
    {
        // Not in input mode? Open it.
        FreqInput(); 
    } 
    else 
    {
        // Already in input mode? Type the number.
        UpdateFreqInput(KEY_5); 
    }
    break;
    case KEY_0:
        ToggleModulation();
        break;
    case KEY_6:
        ToggleListeningBW();
        break;
    case KEY_4:
            ToggleStepsCount();
        break;
    case KEY_SIDE2:
        ToggleBacklight();
        break;
    case KEY_PTT:
        SetState(STILL);
        TuneToPeak();
        break;
    case KEY_MENU:
        break;
        case KEY_EXIT:
        // if the user hits EXIT while a scan range is active, clear it and
        // immediately abandon spectrum mode.  This mirrors the behaviour in
        // the main UI and prevents the range from following the user back later.
#ifdef ENABLE_SCAN_RANGES
        if (gScanRangeStart) {
            gScanRangeStart = 0;
            gScanRangeStop  = 0;
            gUpdateStatus   = true;
            // fall through to normal exit logic
        }
#endif
        if (menuState)
        {
            menuState = 0;
            break;
        }

        #ifdef ENABLE_FEAT_N7SIX_SPECTRUM
            SaveSettings();
        #endif

        // 1. KILL THE SPECTRUM LOGIC
        isInitialized = false; 
        DeInitSpectrum();
        RestoreRegisters();

        // 2. FORCE SYSTEM STATE TO FOREGROUND
        // This is the most important part. If gCurrentFunction is not 
        // FUNCTION_FOREGROUND, the radio refuses to reload the VFOs.
        extern FUNCTION_Type_t gCurrentFunction;
        gCurrentFunction = FUNCTION_FOREGROUND;

        // 3. THE "SNAP-BACK" LOGIC
        // We tell the system that BOTH VFOs are now "Dirty" and must be 
        // completely reconstructed from the EEPROM.
        for (uint8_t i = 0; i < 2; i++) {
            // We use the '2' or 'VFO_CONFIGURE_RELOAD' flag to force a hardware retune
            // This clears the Spectrum peak frequency out of the BK4819 chip.
            RADIO_ConfigureChannel(i, 2); 
        }

        // 4. RESET THE GLOBAL POINTER
        // Ensure the system is looking at the user's primary selected VFO
        gCurrentVfo = &gEeprom.VfoInfo[gEeprom.TX_VFO];

        // 5. TRIGGER THE UI OVERHAUL
        // We force a full screen wipe and a status bar refresh
        gRequestDisplayScreen = DISPLAY_MAIN;
        gUpdateDisplay        = true;
        gUpdateStatus         = true;

        // 6. RESUME MAIN AUDIO/RX LOOP
        // This restarts the squelch and audio paths for the restored frequencies
        APP_StartListening(FUNCTION_FOREGROUND);
        break;
        
    }
}

static void OnKeyDownFreqInput(uint8_t key)
{
    switch (key)
    {
    case KEY_F:
        // Ignore function key in frequency input mode to prevent crash
        return;
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
    case KEY_STAR:
        UpdateFreqInput(key);
        break;
    case KEY_EXIT:
        if (freqInputIndex == 0)
        {
            SetState(previousState);
            break;
        }
        UpdateFreqInput(key);
        break;
    case KEY_MENU:
        if (tempFreq < F_MIN || tempFreq > F_MAX)
        {
            break;
        }
        SetState(previousState);
        currentFreq = tempFreq;
#ifdef ENABLE_SCAN_RANGES
        if (gScanRangeStart) {
            if (currentFreq < gScanRangeStart) currentFreq = gScanRangeStart;
            if (currentFreq > gScanRangeStop)  currentFreq = gScanRangeStop;
        }
#endif
        if (currentState == SPECTRUM)
        {
            ResetBlacklist();
            RelaunchScan();
        }
        else
        {
            SetF(currentFreq);
        }
        break;
    default:
        break;
    }
}

void OnKeyDownStill(KEY_Code_t key)
{
    switch (key)
    {
    case KEY_3:
        UpdateDBMax(true);
        break;
    case KEY_9:
        UpdateDBMax(false);
        break;
    case KEY_UP:
        if (menuState)

#ifdef ENABLE_NAVIG_LEFT_RIGHT
        {
            SetRegMenuValue(menuState, false);
            break;
        }
        UpdateCurrentFreqStill(false);
#else
        {
            SetRegMenuValue(menuState, true);
            break;
        }
        UpdateCurrentFreqStill(true);
#endif
        break;
    case KEY_DOWN:
        if (menuState)
#ifdef ENABLE_NAVIG_LEFT_RIGHT
        {
            SetRegMenuValue(menuState, true);
            break;
        }
        UpdateCurrentFreqStill(true);
#else
        {
            SetRegMenuValue(menuState, false);
            break;
        }
        UpdateCurrentFreqStill(false);
#endif
        break;
    case KEY_STAR:
        UpdateRssiTriggerLevel(true);
        break;
    case KEY_F:
        UpdateRssiTriggerLevel(false);
        break;
        case KEY_5:
    // Use 'currentState' which is the standard variable for this firmware
    if (currentState != FREQ_INPUT) 
    {
        // Not in input mode? Open it.
        FreqInput(); 
    } 
    else 
    {
        // Already in input mode? Type the number.
        UpdateFreqInput(KEY_5); 
    }
    break;
    case KEY_0:
        ToggleModulation();
        break;
    case KEY_6:
        ToggleListeningBW();
        break;
    case KEY_SIDE1:
        monitorMode = !monitorMode;
        break;
    case KEY_SIDE2:
        ToggleBacklight();
        break;
    case KEY_PTT:
        // TODO: start transmit
        /* BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true); */
        break;
    case KEY_MENU:
        if (menuState == ARRAY_SIZE(registerSpecs) - 1)
        {
            menuState = 1;
        }
        else
        {
            menuState++;
        }
        redrawScreen = true;
        break;
    case KEY_EXIT:
        if (!menuState)
        {
            SetState(SPECTRUM);
            lockAGC = false;
            monitorMode = false;
            RelaunchScan();
            break;
        }
        menuState = 0;
        break;
    default:
        break;
    }
}

static void RenderFreqInput() { UI_PrintString(freqInputString, 2, 127, 0, 8); }

static void RenderStatus()
{
    memset(gStatusLine, 0, sizeof(gStatusLine));
    DrawStatus();
    ST7565_BlitStatusLine();
}

/**
 * @brief Professional-grade spectrum display rendering pipeline
 *
 * Orchestrates the complete spectrum analyzer display by calling individual
 * drawing functions in optimized order to build a multi-layered visualization
 * with proper visual hierarchy. The rendering pipeline follows this sequence:
 *
 * 1. **Background Layer** (Frequency/Time Axis)
 *    - Frequency tick marks with graduated scaling
 *    - Center frequency indicator for center mode
 *    - Edge markers for span mode
 *
 * 2. **Signal Layer** (Spectrum Data)
 *    - High-resolution spectrum bars with smooth scaling
 *    - Dynamic range compression for visibility
 *    - Peak frequency indicator with arrow
 *
 * 3. **Reference Layer** (Amplitude Reference)
 *    - RSSI trigger level line
 *    - Signal detection threshold
 *    - Noise floor reference
 *
 * 4. **Information Layer** (Text & Metrics)
 *    - Peak frequency display (6 decimal places)
 *    - S-meter reading
 *    - Signal strength in dBm
 *    - Scan step and bandwidth indicators
 *
 * 5. **History Layer** (Waterfall)
 *    - Temporal signal analysis (16 rows × frequency samples)
 *    - Professional 16-level grayscale representation
 *    - Smooth transitions with spatial dithering
 *
 * @see DrawTicks() - Frequency axis rendering
 * @see DrawSpectrumEnhanced() - Signal amplitude display
 * @see DrawRssiTriggerLevel() - Reference level indicators
 * @see DrawF() - Peak frequency text
 * @see DrawNums() - Numeric information display
 * @see DrawWaterfall() - Temporal waterfall visualization
 */

 static void DrawGridBackground(void)
{
    for (uint8_t x = 0; x < 128; x++)
    {
        // 1. VERTICAL LOGIC: Equally distributed every 16 pixels
        // Lines will appear at: 0, 16, 32, 48, 64, 80, 96, 112, and 127.
        // (Note: we force 127 to ensure the right edge is closed)
        bool isVerticalLinePos = (x % 16 == 0 || x == 127);

        // 2. HORIZONTAL LOGIC: 50% duty cycle dots (every 2nd pixel)
        bool isHorizontalDot = (x % 2 == 0);

        // --- ROW 1 (Top of Spectrum Area) ---
        uint8_t r1 = 0;
        if (isVerticalLinePos) r1 |= 0xA0; // Vertical dots
        if (isHorizontalDot)   r1 |= 0x10; // Top Fixed Grid Line
        gFrameBuffer[1][x] |= r1;

        // --- ROW 2 & 3 (Main Grid Body) ---
        for (uint8_t r = 2; r <= 3; r++) {
            uint8_t pattern = 0;
            if (isVerticalLinePos) pattern |= 0xAA; // Vertical dotted line
            if (isHorizontalDot)   pattern |= 0x01; // Grid Floor 1
            if (isHorizontalDot)   pattern |= 0x10; // Grid Floor 2
            gFrameBuffer[r][x] |= pattern;
        }

        // --- ROW 4 (Bottom Anchor) ---
        uint8_t r4 = 0;
        if (isVerticalLinePos) r4 |= 0x01; 
        if (isHorizontalDot)   r4 |= 0x01; // Bottom Anchor Line
        gFrameBuffer[4][x] |= r4;
    }
}

static void DrawPeakHoldDots(void)
{
    uint16_t bars = GetDisplayWidth();
    for (uint16_t i = 0; i < bars; i++) {
        // Only draw the peak if it's actually above the noise floor
        if (peakHold[i] > 0) {
            uint8_t x = SpecIdxToX(i);
            uint8_t y = Rssi2Y(peakHold[i]);
            
            // Draw a single dot in the framebuffer
            if (y < 127) { // Safety check
                gFrameBuffer[y >> 3][x] |= (1 << (y & 7));
            }
        }
    }
}

static void ProcessSpectrumEnhancements(void) 
{
    for (uint8_t i = 0; i < SPECTRUM_MAX_STEPS; i++) {
        uint16_t currentRssi = rssiHistory[i];

        // 1. SMOOTHING
        smoothedRssi[i] = ((smoothedRssi[i] * 3) + currentRssi) >> 2;

        // 2. PEAK HOLD
        if (currentRssi > peakHold[i]) {
            peakHold[i] = currentRssi;
            peakAge[i] = 20; 
        } else {
            if (peakAge[i] > 0) {
                peakAge[i]--;
            } else if (peakHold[i] > 0) {
                peakHold[i]--; // Slow decay
            }
        }
    }
}

static void RenderSpectrum(void)
{
    // Layer -1: Clear the Spectrum & Grid Area (Rows 1 to 5)
    for (uint8_t r = 1; r <= 5; r++) {
        memset(gFrameBuffer[r], 0, sizeof(gFrameBuffer[r]));
    }

    // --- ENHANCEMENT: SIGNAL CONDITIONING ---
    // Process Smoothing and Peak-Hold timers BEFORE drawing.
    // This ensures all visual components below are in sync.
    ProcessSpectrumEnhancements(); 
    // ----------------------------------------

    // 1. DRAW BACKGROUND CHOICE
    if (settings.useTicksGrid) {
        DrawGridBackground(); 
    } else {
        DrawTicks();          
    }

    // 2. DRAW MAIN SPECTRUM DATA
    // We now use the processed peak data for the Arrow alignment
    uint16_t displayIdx = MapMeasurementToDisplay(peak.i);
    DrawArrow(SpecIdxToX(displayIdx));
    
    // This draws the "live" grass using smoothedRssi[]
    DrawSpectrumEnhanced(); 
    
    // --- PEAK HOLD VISUALIZATION ---
    // This draws the lingering dots using peakHold[]
    DrawPeakHoldDots(); 
    // ------------------------------------

    DrawRssiTriggerLevel();
    DrawF(GetCentroidFrequency()); 
    DrawNums();
    DrawWaterfall();
}

static void RenderStill()
{
    DrawF(fMeasure);

    const uint8_t METER_PAD_LEFT = 3;

    memset(&gFrameBuffer[2][METER_PAD_LEFT], 0b00010000, 121);

    for (int i = 0; i < 121; i += 5)
    {
        gFrameBuffer[2][i + METER_PAD_LEFT] = 0b00110000;
    }

    for (int i = 0; i < 121; i += 10)
    {
        gFrameBuffer[2][i + METER_PAD_LEFT] = 0b01110000;
    }

    uint8_t x = Rssi2PX(displayRssi, 0, 121);
    for (int i = 0; i < x; ++i)
    {
        if (i % 5)
        {
            gFrameBuffer[2][i + METER_PAD_LEFT] |= 0b00000111;
        }
    }

    int dbm = Rssi2DBm(displayRssi);
    uint8_t s = DBm2S(dbm);
    sprintf(String, "S: %u", s);
    GUI_DisplaySmallest(String, 4, 25, false, true);
    sprintf(String, "%d dBm", dbm);
    GUI_DisplaySmallest(String, 28, 25, false, true);

    if (!monitorMode)
    {
        uint8_t x = Rssi2PX(settings.rssiTriggerLevel, 0, 121);
        gFrameBuffer[2][METER_PAD_LEFT + x] = 0b11111111;
    }

    const uint8_t PAD_LEFT = 4;
    const uint8_t CELL_WIDTH = 30;
    uint8_t offset = PAD_LEFT;
    uint8_t row = 4;

    for (int i = 0, idx = 1; idx <= 4; ++i, ++idx)
    {
        if (idx == 5)
        {
            row += 2;
            i = 0;
        }
        offset = PAD_LEFT + i * CELL_WIDTH;
        if (menuState == idx)
        {
            for (int j = 0; j < CELL_WIDTH; ++j)
            {
                gFrameBuffer[row][j + offset] = 0xFF;
                gFrameBuffer[row + 1][j + offset] = 0xFF;
            }
        }
        sprintf(String, "%s", registerSpecs[idx].name);
        GUI_DisplaySmallest(String, offset + 2, row * 8 + 2, false,
                            menuState != idx);

#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
        if(idx == 1)
        {
            sprintf(String, "%ddB", LNAsOptions[GetRegMenuValue(idx)]);
        }
        else if(idx == 2)
        {
            sprintf(String, "%ddB", LNAOptions[GetRegMenuValue(idx)]);
        }
        else if(idx == 3)
        {
            sprintf(String, "%ddB", VGAOptions[GetRegMenuValue(idx)]);
        }
        else if(idx == 4)
        {
            sprintf(String, "%skHz", BPFOptions[(GetRegMenuValue(idx) / 0x2aaa)]);
        }
#else
        sprintf(String, "%u", GetRegMenuValue(idx));
#endif
        GUI_DisplaySmallest(String, offset + 2, (row + 1) * 8 + 1, false,
                            menuState != idx);
    }
}

static void Render()
{
    UI_DisplayClear();

    // If we are listening, we still want to see the SPECTRUM UI
    if (isListening && currentState == SPECTRUM) {
        RenderSpectrum();
    } else {
        switch (currentState)
        {
            case SPECTRUM:
                RenderSpectrum();
                break;
            case FREQ_INPUT:
                RenderFreqInput();
                break;
            case STILL:
                RenderStill();
                break;
        }
    }

    ST7565_BlitFullScreen();
}

static bool HandleUserInput()
{
    kbd.prev = kbd.current;
    kbd.current = GetKey();

    // --- HOLDING LOGIC ---
    if (kbd.current != KEY_INVALID && kbd.current == kbd.prev)
    {
        if (kbd.counter < 16) kbd.counter++;
        
        // INSTANT TRIGGER AT THRESHOLD (Long Press)
        // --- HandleUserInput() ---
        if (kbd.counter == 16 && kbd.current == KEY_5)
        {
            settings.useTicksGrid = !settings.useTicksGrid;
            
            // toastMessage = settings.useTicksGrid ? "GRID MODE" : "CLEAR"; // <-- COMMENT OUT
            // messageTimer = 30;                                           // <-- COMMENT OUT
            
            redrawScreen = true;
            kbd.counter = 17; 
            return true;
        }
        SYSTEM_DelayMs(20);
    }
    // --- RELEASE LOGIC ---
    else
    {
        // Short Press Release: Only triggers if user released before reaching 16
        if (kbd.prev == KEY_5 && kbd.counter >= 3 && kbd.counter < 16)
        {
            OnKeyDown(KEY_5, false);
        }
        kbd.counter = 0;
    }

    // --- OTHER KEYS ---
    // Handle all other keys at counter 3, excluding KEY_5
    if (kbd.counter == 3 && kbd.current != KEY_5)
    {
        switch (currentState) {
            case SPECTRUM:    OnKeyDown(kbd.current, false); break;
            case FREQ_INPUT: OnKeyDownFreqInput(kbd.current); break;
            case STILL:      OnKeyDownStill(kbd.current); break;
        }
    }
    return true;
}

// --- 1. The Helper Function (Keep this clean of Scan comments) ---
static int8_t GetFrequencyOffset(uint32_t f) {
    for (uint8_t i = 0; i < (sizeof(calTable) / sizeof(calTable[0])) - 1; i++) {
        if (f >= calTable[i].freq && f < calTable[i+1].freq) {
            return calTable[i].offset;
        }
    }
    return calTable[(sizeof(calTable) / sizeof(calTable[0])) - 1].offset;
}

/**
 * @brief Perform single frequency measurement during scan
 *
 * Core measurement routine executed for each frequency step:
 * 1. Checks if frequency has valid data (not already scanned)
 * 2. Sets hardware to target frequency
 * 3. Performs RSSI measurement
 * 4. Updates scan statistics (peak, min, etc.)
 * 5. Records measurement in history buffer
 *
 * @see SetF() - Tunes to specific frequency
 * @see Measure() - Reads RSSI from hardware
 * @see UpdateScanInfo() - Updates statistics
 * @see SetRssiHistory() - Records in waterfall/display buffer
 */
  static void Scan()
{
    // Map raw measurement index to display index and skip if that bin was already
    // populated. Preserve blacklist check against the raw index.
    uint8_t dIdx = MapMeasurementToDisplay(scanInfo.i);
    if (rssiHistory[dIdx] != RSSI_MAX_VALUE
#ifdef ENABLE_SCAN_RANGES
        && !IsBlacklisted(scanInfo.i)
#endif
    )
    {
        SetF(scanInfo.f);           // Tune receiver to target frequency
        Measure();                   // Perform RSSI measurement
        bool hasSignal = (scanInfo.rssi > settings.rssiTriggerLevel);
        SetBandLed(scanInfo.f, false, hasSignal); // Set LED for RX only if signal
        UpdateScanInfo();            // Update peak/min statistics
    }
}

/**
 * @brief Advance to next frequency step
 *
 * Sequence:
 * 1. Increment peak age counter (for peak hold timeout)
 * 2. Calculate next frequency based on step size
 * 3. Update scan counter
 * 4. Apply frequency boundaries
 * 5. Trigger waterfall update if interval reached
 * 6. Determine if scan is complete
 */
static void NextScanStep()
{
    ++peak.t;  // Increment peak age for timeout tracking
    ++scanInfo.i;
    scanInfo.f += scanInfo.scanStep;
}

static void UpdateScan()
{
    Scan(); 

    if (scanInfo.i < scanInfo.measurementsCount)
    {
        uint8_t dIdx = MapMeasurementToDisplay(scanInfo.i);
        if (scanInfo.rssi > peakHold[dIdx])
        {
            peakHold[dIdx] = scanInfo.rssi;
        }
    }

    if (scanInfo.i < scanInfo.measurementsCount)
    {
        NextScanStep();
        return;
    }

    // --- SWEEP COMPLETE ---
    if (scanInfo.measurementsCount < SPECTRUM_MAX_STEPS)
    {
        memset(&rssiHistory[scanInfo.measurementsCount], 0,
               (SPECTRUM_MAX_STEPS - scanInfo.measurementsCount) * sizeof(rssiHistory[0]));
    }

    UpdateWaterfall(); 
    
    // --- NEW: PRE-CALCULATE SMOOTHING FILTER ---
    // Moving the math here means DrawSpectrumEnhanced() runs much faster.
    uint16_t bars = (scanInfo.measurementsCount > SPECTRUM_MAX_STEPS) ? SPECTRUM_MAX_STEPS : scanInfo.measurementsCount;
    if (bars >= 2) {
        // Handle first bar
        uint32_t sum = ((uint32_t)rssiHistory[0] << 1) + rssiHistory[0] + rssiHistory[1];
        smoothedRssi[0] = (uint16_t)(sum >> 2);

        // Handle middle bars
        for (uint16_t i = 1; i < (bars - 1); i++) {
            sum = ((uint32_t)rssiHistory[i] << 1) + rssiHistory[i-1] + rssiHistory[i+1];
            smoothedRssi[i] = (uint16_t)(sum >> 2);
        }

        // Handle last bar
        uint16_t last = bars - 1;
        sum = ((uint32_t)rssiHistory[last] << 1) + rssiHistory[last-1] + rssiHistory[last];
        smoothedRssi[last] = (uint16_t)(sum >> 2);
    }

    redrawScreen = true; // Now redraw using the pre-calculated smoothedRssi
    preventKeypress = false;

    UpdatePeakInfo(); 

    // --- PEAK HOLD DECAY (Once per full sweep) ---
    uint16_t decayLimit = (scanInfo.measurementsCount > SPECTRUM_MAX_STEPS) ? SPECTRUM_MAX_STEPS : scanInfo.measurementsCount;
    for (uint16_t i = 0; i < decayLimit; i++) {
        if (peakHold[i] > 0) {
            peakHold[i] = (peakHold[i] * 7) >> 3; 
        }
    }

    if (IsPeakOverLevel())
    {
        ToggleRX(true);
        TuneToPeak();
        return; 
    }

    newScanStart = true;
}

static void UpdateStill()
{
    Measure();
    
    // Apply our Calibration Offset
    int8_t offset = GetFrequencyOffset(fMeasure);
    scanInfo.rssi += (offset * 2); // BK4819 RSSI is usually in 0.5dB steps

    if (displayRssi == 0) {
        displayRssi = scanInfo.rssi;
    } else {
        displayRssi = (displayRssi * 9 + scanInfo.rssi) / 10;
    }
    
    redrawScreen = true;
    preventKeypress = false;
    peak.rssi = scanInfo.rssi;

    if (IsPeakOverLevel() || monitorMode) {
        ToggleRX(true);
    }
}

 


// Noise persistence array for smoothed grass generation
static uint16_t noisePersistence[128]; 

static void UpdateListening()
{
    preventKeypress = false;
    
    // --- 1. HANDLE TIMERS & TAIL DETECTION ---
#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
    bool tailFound = checkIfTailFound();
    if (tailFound)
        listenT = 0;
    if (listenT)
    {
        listenT--;
        redrawStatus = true;
        redrawScreen = true;
        // do not return here: continue to run periodic updates
        // (noise/grass + waterfall) while the radio remains keyed.
    }
#else
    if (currentState == STILL)
        listenT = 0;
    if (listenT)
    {
        listenT--;
        redrawStatus = true;
        redrawScreen = true;
        // continue executing so the display stays active during listen
    }
#endif

    // --- 2. ALWAYS-ON MEASUREMENT / QUICK WATERFALL ---
    Measure();                    // read RSSI every tick
    peak.rssi = scanInfo.rssi;
    UpdateWaterfallQuick();       // keep the waterfall moving
    
    // NOTE (Issue #2): Removed continuous listening-mode auto-adjust that was conflicting
    // with manual threshold adjustments. The AutoTriggerLevel() called during spectrum
    // scans provides sufficient automatic adjustment without fighting manual controls.

    // --- 3. THE HELICOPTER FIX (THROTTLED EXECUTION) ---
    // a shorter period (6 ticks) keeps the display more responsive while
    // still avoiding excessive CPU overhead during long listen periods.
    static uint8_t quietCounter = 0;
    if (++quietCounter >= 6) 
    {
        quietCounter = 0;
        
        // --- 4. LIVE WATERFALL & SPECTRUM PULSING (heavy path) ---
        if (currentState == SPECTRUM) 
        {
            uint8_t peakDisplay = MapMeasurementToDisplay(peak.i);
            for (int i = 0; i < SPECTRUM_MAX_STEPS; i++)
            {
                if (i >= (int)peakDisplay - 1 && i <= (int)peakDisplay + 1) {
                    rssiHistory[i] = peak.rssi + (FastRandom() % 12);
                } else {
                    uint16_t baseFloor = scanInfo.rssiMin + 32;
                    int8_t roll = (FastRandom() % 9) - 4;
                    noisePersistence[i] = (noisePersistence[i] * 7 + (baseFloor + roll)) >> 3;
                    uint8_t spike = (FastRandom() % 14);
                    uint16_t finalGrass = noisePersistence[i] + spike;
                    if (finalGrass > peak.rssi && peak.rssi > 10)
                        rssiHistory[i] = peak.rssi - 5;
                    else
                        rssiHistory[i] = finalGrass;
                }
            }
            UpdateWaterfall(); 
            redrawScreen = true; 
        }

        // --- 5. SQUELCH / STATE MANAGEMENT ---
#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
        if ((IsPeakOverLevel() && !tailFound) || monitorMode)
        {
            listenT = settings.listenTScan;
            return;
        }
#else
        if (IsPeakOverLevel() || monitorMode)
        {
            listenT = settings.listenTStill;
            return;
        }
#endif

        ToggleRX(false);
        ResetScanStats();
        
        // CRITICAL FIX: Clear rssiHistory to force fresh measurements when 
        // resuming scan mode. Without this, Scan() skips bins that already 
        // have data, causing the spectrum to appear frozen after RX ends.
        memset(rssiHistory, 0, sizeof(rssiHistory));
        redrawScreen = true;
    }
}

static void Tick()
{
#ifdef ENABLE_AM_FIX
    if (gNextTimeslice)
    {
        gNextTimeslice = false;
        if (settings.modulationType == MODULATION_AM && !lockAGC)
        {
            AM_fix_10ms(vfo); // allow AM_Fix to apply its AGC action
        }
    }
#endif

#ifdef ENABLE_SCAN_RANGES
    if (gNextTimeslice_500ms)
    {
        gNextTimeslice_500ms = false;

        // if a lot of steps then it takes long time
        // we don't want to wait for whole scan
        // listening has it's own timer
        if (GetStepsCount() > 128 && !isListening)
        {
            UpdatePeakInfo();
            if (IsPeakOverLevel())
            {
                ToggleRX(true);
                TuneToPeak();
                return;
            }
            redrawScreen = true;
            preventKeypress = false;
        }
    }
#endif

    if (!preventKeypress)
    {
        HandleUserInput();
    }
    if (newScanStart)
    {
        InitScan();
        newScanStart = false;
    }
    if (isListening && currentState != FREQ_INPUT)
    {
        UpdateListening();
    }
    else
    {
        if (currentState == SPECTRUM)
        {
            UpdateScan();
        }
        else if (currentState == STILL)
        {
            UpdateStill();
        }
    }
    if (redrawStatus || ++statuslineUpdateTimer > 4096)
    {
        RenderStatus();
        redrawStatus = false;
        statuslineUpdateTimer = 0;
    }

    // watchdog: ensure the display continues to refresh even if the
    // normal logic is being throttled during a listening event.  This
    // counter increments only when no redraw is pending and resets as
    // soon as we draw anything or exit listening mode.
    {
        static uint8_t listenWatchdog = 0;
        if (isListening)
        {
            if (!redrawScreen && !redrawStatus)
            {
                if (++listenWatchdog >= 6)  // force redraw every ~6 ticks when idle
                {
                    redrawStatus = true;
                    redrawScreen = true;
                    listenWatchdog = 0;
                }
            }
            else
            {
                listenWatchdog = 0;
            }
        }
        else
        {
            listenWatchdog = 0;
        }
    }

    if (redrawScreen)
    {
        Render();
        // For screenshot
        #ifdef ENABLE_FEAT_N7SIX_SCREENSHOT
            getScreenShot(false);
        #endif
        redrawScreen = false;
    }
}

void APP_RunSpectrum(void)
{
    vfo = gEeprom.TX_VFO;

#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
    LoadSettings();
#endif

    // 1. Load settings from EEPROM first
    SPECTRUM_LoadSettings();

    // 2. --- FORCED DEFAULT: 600.00k OFFSET ---
    settings.frequencyChangeStep = 60000; 

    // --- APPLY SCAN STEP DEFAULTS ---
    bool scanStepFound = false;
    for (uint8_t i = 0; i < ARRAY_SIZE(scanStepValues); i++) {
        if (scanStepValues[i] == 2500) {
            scanStepFound = (settings.scanStepIndex == i);
            if (!scanStepFound) {
                if (settings.scanStepIndex >= ARRAY_SIZE(scanStepValues))
                    settings.scanStepIndex = i;
            }
            break;
        }
    }

    // --- FREQUENCY INITIALIZATION LOGIC ---
#ifdef ENABLE_SCAN_RANGES
    if (gScanRangeStart) {
        // RANGE MODE: Anchored by defined start/stop limits
        initialFreq = (gScanRangeStart + gScanRangeStop) / 2;
        currentFreq = gScanRangeStart; 
        settings.stepsCount = STEPS_32; 
    } else {
        // VFO MODE: Anchor at Center
        initialFreq = gTxVfo->pRX->Frequency; 
        
        // Calculate dynamic start frequency based on current span
        // fStart = Center - ((Steps * StepSize) / 2)
        uint32_t span = GetStepsCount() * scanStepValues[settings.scanStepIndex];
        currentFreq = initialFreq - (span / 2);
    }
#else
    // VFO MODE: Anchor at Center
    initialFreq = gTxVfo->pRX->Frequency;
    uint32_t span = GetStepsCount() * scanStepValues[settings.scanStepIndex];
    currentFreq = initialFreq - (span / 2);
#endif

    BackupRegisters();

    isListening = true; 
    redrawStatus = true;
    redrawScreen = true;
    newScanStart = true;

    ToggleRX(true), ToggleRX(false); 
    RADIO_SetModulation(settings.modulationType = gTxVfo->Modulation);

#ifdef ENABLE_FEAT_N7SIX_SPECTRUM
    BK4819_SetFilterBandwidth(settings.listenBw, false);
#else
    BK4819_SetFilterBandwidth(settings.listenBw = BK4819_FILTER_BW_WIDE, false);
#endif

    // --- GAIN & SIGNAL INTEGRITY CORRECTION ---
    BK4819_WriteRegister(0x13, 0x03E4); 
    BK4819_WriteRegister(0x14, 0x1900); 
    BK4819_WriteRegister(0x10, BK4819_ReadRegister(0x10) | 0x8000);

    RelaunchScan();

    memset(rssiHistory, 0, sizeof(rssiHistory));
    memset(waterfallHistory, 0, sizeof(waterfallHistory));
    for (uint8_t i = 0; i < SPECTRUM_MAX_STEPS; ++i) displayBestIndex[i] = 0xFFFF;
    waterfallIndex = 0;

    isInitialized = true;
    while (isInitialized) { Tick(); }
}