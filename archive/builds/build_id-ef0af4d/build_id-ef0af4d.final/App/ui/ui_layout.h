/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
 *
 * UI Layout Constants for UV-K1Series ApeX-Edition
 *
 * This header centralizes all magic numbers used in UI layout calculations.
 * Changing a constant here updates all affected UI elements consistently.
 */

#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <stdint.h>

/* ============================================================================
 * FONT METRICS
 * ============================================================================ */

#define UI_FONT_SMALL_WIDTH      7       // Width of small font characters (gFontSmall)
#define UI_FONT_SMALL_SPACING    (UI_FONT_SMALL_WIDTH + 1)  // 8px per char including gap
#define UI_FONT_BIG_WIDTH        8       // Width of big font characters (gFontBig)
#define UI_FONT_BIG_SPACING      (UI_FONT_BIG_WIDTH + 1)    // 9px per char including gap
#define UI_FONT_TINY_WIDTH       3       // Width of 3x5 tiny font characters
#define UI_FONT_TINY_SPACING     4       // 4px per char (3px + 1px gap)

/* ============================================================================
 * DISPLAY GEOMETRY
 * ============================================================================ */

#define UI_LCD_WIDTH             128     // LCD width in pixels
#define UI_LCD_HEIGHT            64      // LCD height in pixels
#define UI_FRAME_LINES           8       // Number of 8-pixel-tall frame lines
#define UI_STATUS_LINE_Y          0      // Status bar is always line 0
#define UI_MAIN_TEXT_LINE_DUAL    3       // Line for text when dual VFO is active
#define UI_MAIN_TEXT_LINE_MAIN    5       // Line for text when main-only mode
#define UI_MAIN_TEXT_Y_DUAL       25      // Y position for dual VFO text (tiny font)
#define UI_MAIN_TEXT_Y_MAIN       41      // Y position for main-only text (tiny font)

/* ============================================================================
 * STATUS BAR LAYOUT
 * ============================================================================ */

#define UI_STATUS_X_TIMER         39      // X position after timer indicator
#define UI_STATUS_X_INDICATOR     8       // X position after first indicator (NOAA/PS)
#define UI_STATUS_X_SCAN          10      // Width of scan indicator
#define UI_STATUS_X_VOICE         (sizeof(BITMAP_VoicePrompt))  // Voice indicator width
#define UI_STATUS_X_PTT           3       // Extra spacing after PTT indicator
#define UI_STATUS_X_VOX           3       // Extra spacing after VOX indicator
#define UI_STATUS_X_KEY_MIN       69      // Minimum X position for key/backlight indicators
#define UI_STATUS_X_BATTERY_BASE   (UI_LCD_WIDTH - sizeof(BITMAP_BatteryLevel1))  // Battery base X
#define UI_STATUS_DEBUG_WIDTH     16      // Width of debug value display

/* ============================================================================
 * MAIN SCREEN LAYOUT
 * ============================================================================ */

#define UI_MAIN_BAR_X             2       // X position of audio/RSSI bar
#define UI_MAIN_BAR_WIDTH         125     // Width of bar clear region
#define UI_MAIN_BAR_MAX_LEVELS    25      // Maximum number of bar levels
#define UI_MAIN_PRIORITY_X_OFFSET 11      // X offset for priority label
#define UI_MAIN_SCAN_SPARKLINE_X  7       // X position of scan RSSI sparkline

/* ============================================================================
 * AUDIO SCOPE LAYOUT
 * ============================================================================ */

#define UI_SCOPE_SAMPLES          43      // Number of columns (43 × 3px = 128px wide)
#define UI_SCOPE_NOISE_GATE       50      // Minimum range for display
#define UI_SCOPE_FLOOR_RISE       2       // Floor rise per frame
#define UI_SCOPE_FLOOR_DROP_SHR   3       // Floor drop IIR shift
#define UI_SCOPE_VOLUME_MIN       200     // Minimum volume at silence

/* ============================================================================
 * SCAN PROGRESS LAYOUT
 * ============================================================================ */

#define UI_SCAN_GAUGE_LEFT        (width * 8 + 9)  // Dynamic, width-dependent
#define UI_SCAN_GAUGE_RIGHT       126     // Right edge of gauge
#define UI_SCAN_GAUGE_FILL_START  (UI_SCAN_GAUGE_LEFT + 2)
#define UI_SCAN_GAUGE_FILL_END    (UI_SCAN_GAUGE_RIGHT - 2)

/* ============================================================================
 * FREQUENCY DISPLAY
 * ============================================================================ */

#define UI_FREQ_DIGIT_WIDTH       13      // Width per big digit
#define UI_FREQ_DOT_WIDTH         3       // Width of decimal point

/* ============================================================================
 * MENU LAYOUT
 * ============================================================================ */

#define UI_MENU_LIST_WIDTH        6       // Max characters in menu list (left side)
#define UI_MENU_ITEM_X1           (8 * UI_MENU_LIST_WIDTH + 2)  // Left edge of menu items
#define UI_MENU_ITEM_X2           (UI_LCD_WIDTH - 1)            // Right edge of menu items
#define UI_MENU_SEPARATOR_X       (8 * UI_MENU_LIST_WIDTH + 1)  // Vertical dotted line

/* ============================================================================
 * MISCELLANEOUS
 * ============================================================================ */

#define UI_POPUP_BORDER_X1        10      // Popup left border
#define UI_POPUP_BORDER_X2        117     // Popup right border
#define UI_POPUP_BORDER_Y1        10      // Popup top border
#define UI_POPUP_BORDER_Y2        37      // Popup bottom border
#define UI_POPUP_TEXT_X           9       // Popup text X position
#define UI_POPUP_TEXT_Y           2       // Popup text line (Y / 8)

#define UI_UNLOCK_KEYBOARD_X      12      // X position for "UNLOCK KEYBOARD" text
#define UI_UNLOCK_KEYBOARD_SHIFT  6       // Line shift for unlock text

#endif /* UI_LAYOUT_H */