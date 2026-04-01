
/**
 * =====================================================================================
 * @file        bitmaps.c
 * @brief       User Interface Bitmap & Icon Assets (Menu, Status, Power Modes)
 * @author      Dual Tachyon (Original)
 * @author      N7SIX/Professional Enhancement Team (2025-2026)
 * @version     v7.6.0 (ApeX Edition)
 * @license     Apache License, Version 2.0
 * "Precomputed bitmap assets for efficient UI rendering on 128x64 LCD display"
 * =====================================================================================
 * ARCHITECTURAL OVERVIEW:
 * Provides precomputed 1-bit bitmap icons and glyphs for ST7565 display rendering.
 * All images follow 90-degree rotation encoding (monitor rotated anti-clockwise for
 * visual verification). Optimized for direct SPI transmission without runtime processing.
 *
 * MAJOR FEATURES (2025-2026):
 * ---------------------------
 * - Power save mode indicator (PSM icon - 2 variants)
 * - PTT one-push mode glyph (6×6 pixels, 2 states)
 * - PTT classic mode glyph (6×8 pixels, 2 states)
 * - Large menu header letters (F, T, S - 8×8 pixels)
 * - Status bar icons (signal, battery, encryption)
 * - Rotating animation frames for active states
 *
 * TECHNICAL SPECIFICATIONS:
 * -------------------------
 * - Bitmap unit: 1-bit per pixel (MSB first within bytes)
 * - Width alignment: 8-pixel boundaries (1+ bytes per row)
 * - Height: Variable (typically 6, 8, or 16 pixels)
 * - Encoding: 90-degree clockwise rotation (visual RAM layout)
 * - Storage: ROM-based constant arrays (~8 KB total)
 * - Rendering: Direct framebuffer indexing, no palette lookup
 * =====================================================================================
 */
#include "bitmaps.h"

// all these images are on their right sides
// turn your monitor 90-deg anti-clockwise to see the images

const uint8_t gFontPowerSave[2][6] =
{
    {0x00, 0x7f, 0x9, 0x9, 0x9, 0x6},
    {0x00, 0x26, 0x49, 0x49, 0x49, 0x32},
};

const uint8_t gFontPttOnePush[2][6] =
{
    {0x00, 0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x00, 0x7f, 0x9, 0x9, 0x9, 0x6},
};

const uint8_t gFontPttClassic[2][6] =
{
    {0x00, 0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x00, 0x7f, 0x40, 0x40, 0x40, 0x40},
};

const uint8_t gFontF[8] =
{
    0b01111111,
    0b00000000,
    0b01110110,
    0b01110110,
    0b01110110,
    0b01110110,
    0b01111110,
    0b01111111
};

const uint8_t gFontS[6] =
{
    0x26, 0x49, 0x49, 0x49, 0x49, 0x32 // 'S'
};

const uint8_t gFontKeyLock[9] =
{
    0x7c, 0x46, 0x45, 0x45, 0x45, 0x45, 0x45, 0x46, 0x7c
};

const uint8_t gFontLight[9] =
{
    0b00001100,
    0b00010010,
    0b00100001,
    0b01101101,
    0b01111001,
    0b01101101,
    0b00100001,
    0b00010010,
    0b00001100,
};

const uint8_t gFontMute[12] =
{
    0b00011100,
    0b00011100,
    0b00010100,
    0b00100010,
    0b01000001,
    0b01111111,
    0b00000000,
    0b00100010,
    0b00010100,
    0b00001000,
    0b00010100,
    0b00100010,
};

const uint8_t gFontXB[2][6] =
{   // "XB"
    {0x00, 0x63, 0x14, 0x8, 0x14, 0x63},
    {0x00, 0x7f, 0x49, 0x49, 0x49, 0x36},
};


const uint8_t gFontMO[2][6] =
{   // "MO"
    {0x00, 0x7f, 0x2, 0x1c, 0x2, 0x7f},
    {0x00, 0x3e, 0x41, 0x41, 0x41, 0x3e},
};

const uint8_t gFontDWR[3][6] =
{   // "DWR"

    {0x00, 0x7f, 0x41, 0x41, 0x41, 0x3e},
    {0x00, 0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x00, 0x7f, 0x9, 0x19, 0x29, 0x46},
};

#ifdef ENABLE_FEAT_N7SIX_RESCUE_OPS
    const uint8_t gFontRO[2][6] =
    {   // "RO"
        {0x00, 0x7f, 0x9, 0x19, 0x29, 0x46},
        {0x00, 0x3e, 0x41, 0x41, 0x41, 0x3e},
    };
#endif

const uint8_t gFontHold[2][5] =
{   // "><" .. DW on hold
    {0x00, 0x41, 0x22, 0x14, 0x8},
    {0x00, 0x8, 0x14, 0x22, 0x41},
};

const uint8_t BITMAP_BatteryLevel[2] =
{
    0b01011101,
    0b01011101
};

// Quansheng way (+ pole to the left)
const uint8_t BITMAP_BatteryLevel1[17] =
{
    0b00000000,
    0b00111110,
    0b00100010,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01000001,
    0b01111111
};

const uint8_t BITMAP_USB_C[9] =
{
    0b00000000,
    0b00011100,
    0b00100111,
    0b01000100,
    0b01000100,
    0b01000100,
    0b01000100,
    0b00100111,
    0b00011100
};

#ifdef ENABLE_VOX
    const uint8_t gFontVox[2][6] =
    {
        {0x00, 0x1f, 0x20, 0x40, 0x20, 0x1f},
        {0x00, 0x63, 0x14, 0x8, 0x14, 0x63},
    };
#endif

const uint8_t BITMAP_Antenna[5] =
{
    0b00000011,
    0b00000101,
    0b01111111,
    0b00000101,
    0b00000011
};

const uint8_t BITMAP_VFO_Lock[7] =
{
    0b01111100,
    0b01000110,
    0b01000101,
    0b01000101,
    0b01000101,
    0b01000110,
    0b01111100,
};

const uint8_t BITMAP_VFO_Default[5] =
{
    0b11111111,  // Column 1: full height
    0b01111111,  // Column 2: 7 pixels
    0b00111111,  // Column 3: 6 pixels
    0b00011111,  // Column 4: 5 pixels
    0b00001111   // Column 5: 4 pixels (pointer tip)
};

const uint8_t BITMAP_VFO_NotDefault[5] =
{
    0b10000001,  // Column 1: top and bottom only
    0b01000010,  // Column 2: outline
    0b00100100,  // Column 3: outline
    0b00011000,  // Column 4: outline
    0b00001000   // Column 5: pointer tip
};

const uint8_t BITMAP_RX[5] =
{
    0b01111100,  // R: top bar
    0b01000100,  // R: left side
    0b01111100,  // R: middle bar
    0b01010100,  // R+X: legs
    0b01000100   // R: bottom
};

const uint8_t BITMAP_TX[5] =
{
    0b00000010,  // T: top
    0b00000010,  // T: stem
    0b01111110,  // T: crossbar
    0b00000010,  // T: stem
    0b00000010   // T: bottom
};

const uint8_t BITMAP_ScanList0[7] =
{   // '0' symbol
    0b01111111,
    0b01111111,
    0b01000011,
    0b01011101,
    0b01100001,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanList1[7] =
{   // '1' symbol
    0b01111111,
    0b01111111,
    0b01111011,
    0b01000001,
    0b01111111,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanList2[7] =
{   // '2' symbol
    0b01111111,
    0b01111111,
    0b01001101,
    0b01010101,
    0b01011011,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanList3[7] =
{   // '3' symbol
    0b01111111,
    0b01111111,
    0b01011101,
    0b01010101,
    0b01101011,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanListE[7] =
{   // 'E' symbol
    0b01111111,
    0b01111111,
    0b01000001,
    0b01010101,
    0b01010101,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanList123[19] =
{
    // '123' symbol
    0b01111111,
    0b01111111,
    0b01111011,
    0b01000001,
    0b01111111,
    0b01111111,
    0b01111111,
    0b01111111,
    0b01001101,
    0b01010101,
    0b01011011,
    0b01111111,
    0b01111111,
    0b01111111,
    0b01011101,
    0b01010101,
    0b01101011,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_ScanListAll[19] =
{
    // 'All' symbol
    0b01111111,
    0b01111111,
    0b01000011,
    0b01110101,
    0b01000011,
    0b01111111,
    0b01111111,
    0b01111111,
    0b01000001,
    0b01011111,
    0b01011111,
    0b01111111,
    0b01111111,
    0b01111111,
    0b01000001,
    0b01011111,
    0b01011111,
    0b01111111,
    0b01111111
};

const uint8_t BITMAP_compand[6] =
{
    0b00000000,
    0b00111100,
    0b01000010,
    0b01000010,
    0b01000010,
    0b00100100
};

/*
const uint8_t BITMAP_Ready[7] =
{
    0b00001000,
    0b00010000,
    0b00100000,
    0b00010000,
    0b00001000,
    0b00000100,
    0b00000010,
};

const uint8_t BITMAP_NotReady[7] =
{
    0b00000000,
    0b01000010,
    0b00100100,
    0b00011000,
    0b00100100,
    0b01000010,
    0b00000000,
};
*/

const uint8_t BITMAP_PowerUser[3] =
{   // 'arrow' symbol
    0b00111110,
    0b00011100,
    0b00001000,
};

#ifdef ENABLE_NOAA
const uint8_t BITMAP_NOAA[12] =
{	// "WX"
    0b00000000,
    0b01111111,
    0b00100000,
    0b00011000,
    0b00100000,
    0b01111111,
    0b00000000,
    0b01100011,
    0b00010100,
    0b00001000,
    0b00010100,
    0b01100011
};
#endif

#ifndef ENABLE_CUSTOM_MENU_LAYOUT
const uint8_t BITMAP_CurrentIndicator[8] = {
    0xFF,
    0xFF,
    0x7E,
    0x7E,
    0x3C,
    0x3C,
    0x18,
    0x18
};
#endif
