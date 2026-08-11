/* Copyright 2026 NR7Y
 * https://github.com/briand
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

// CW Iambic Keyer implementation
// Ported from NR7Y UV-K5 (dp32g030) to ApeX UV-K1 (PY32F071)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "app/cwkeyer.h"
#include "app/cwhardware.h"
#include "app/cwmacro.h"
#include "audio.h"
#include "settings.h"
#include "misc.h"
#include "scheduler.h"
#include "driver/systick.h"
#include "driver/system.h"
#include "driver/i2c.h"
#include "driver/keyboard.h"
#include "driver/backlight.h"
#include "external/printf/printf.h"

// Debug logging control
#define CW_KEYER_DEBUG 0

// Timer scale: 10ms tick from gGlobalSysTickCounter (100 Hz tick rate)
#define DITS_PER_WORD 50
#define TICKS_PER_MINUTE 6000

// Optional element deadline extension applied when the speaker is off.
// This relaxes the feel of the keying rhythm but exceeds strict ITU WPM timing.
// Set to 0 for standards-compliant timing, or make this a menu setting later.
#ifndef CW_ELEM_DEADLINE_EXTRA_MS
#define CW_ELEM_DEADLINE_EXTRA_MS 20
#endif

// Keyer FSM states
typedef enum {
    CWK_STATE_IDLE = 0,
    CWK_STATE_ACTIVE_ELEMENT,
    CWK_STATE_INTER_ELEMENT_GAP,
    CWK_STATE_INTER_CHAR_GAP,
    CWK_STATE_INTER_WORD_GAP,
    CWK_STATE_EMIT_NONE,
} CW_KeyerFSMState_t;

static CW_KeyerFSMState_t s_KeyerFSMState = CWK_STATE_IDLE;

// Semi-automatic bug keyer FSM states
typedef enum {
    BUG_STATE_IDLE = 0,
    BUG_STATE_DIT_ELEMENT,
    BUG_STATE_DIT_GAP,
    BUG_STATE_DAH_HOLD,
    BUG_STATE_CHAR_GAP,
    BUG_STATE_WORD_GAP,
} CW_BugFSMState_t;

static CW_BugFSMState_t s_bug_state       = BUG_STATE_IDLE;
static uint16_t         s_bug_phase_start = 0;

// Internal keyer runtime state
static uint16_t       s_dit_count  = 0;
static uint16_t       s_dah_count  = 0;
static uint16_t       s_gap_count  = 0;
static uint16_t       s_char_gap_count = 0;
static uint16_t       s_word_gap_count = 0;
static uint16_t       s_elem_start_count = 0;
static bool           s_active_is_dit = false;
static bool           s_pending_alternate = false;
static bool           s_last_handkey_ptt = false;
static uint16_t       s_elem_deadline_extra_ms = 0;
// Playback buffer for decoded ASCII. Worst case: every char has a space prefix,
// so size must accommodate up to CW_MACRO_MAX_LEN * 2 characters + NUL.
static char s_playback_buf[CW_MACRO_MAX_LEN * 2 + 1];
static uint16_t s_playback_buf_len = 0;
static uint16_t s_playback_pos = 0;
static uint8_t s_play_char_pattern = 0;
static uint8_t s_play_char_len = 0;
static uint8_t s_play_elem_index = 0;

// Playback FSM states
typedef enum {
    PB_STATE_IDLE = 0,
    PB_STATE_ACTIVE_ELEMENT,
    PB_STATE_INTER_ELEMENT_GAP,
    PB_STATE_INTER_CHAR_GAP,
    PB_STATE_INTER_WORD_GAP,
} PB_State_t;
static PB_State_t s_pb_state = PB_STATE_IDLE;
static bool s_play_space_pending = false;

static volatile bool s_cfg_dirty = true;
static volatile bool s_enable_keyer = false;
static uint8_t s_last_key_input_mode = 0xFF;

// Global variables for keyer state (ApeX-specific)
bool gCW_KeyerManagesPtt = false;
bool gCW_KeyerUsingSD1 = false;

#ifdef ENABLE_FLASHLIGHT
bool gCW_FlashlightSending = false;
#endif

void CW_KeyerResetRuntime(void)
{
    s_KeyerFSMState = CWK_STATE_EMIT_NONE;
    s_elem_start_count = (uint16_t)gGlobalSysTickCounter;
    s_active_is_dit = false;
    s_pending_alternate = false;
    s_last_handkey_ptt = false;
    s_bug_state = BUG_STATE_IDLE;
    s_bug_phase_start = 0;
    CW_HW_ResetKeySamples();
}

static void CW_KeyerDeinit()
{
    CW_KeyerResetRuntime();
    CW_ConfigureADCforCECPaddles(false);
    CW_ConfigurePortRing(false);
    gCW_KeyerManagesPtt = false;
    gCW_KeyerUsingSD1 = false;
    s_enable_keyer = false;
    s_last_key_input_mode = 0xFF;
}

void CW_KeyerReconfigure(bool enable)
{
#if CW_KEYER_DEBUG
    if (enable) {
        // Debug output if needed
    }
#endif

    if (!enable) {
        CW_KeyerDeinit();
        s_cfg_dirty = false;
        return;
    }

    gCW_KeyerManagesPtt = true;
    gCW_KeyerUsingSD1 |= gEeprom.CW_KEY_INPUT & CW_KEY_FLAG_SIDE1;
    CW_KeyerResetRuntime();
    s_cfg_dirty = true;
}

void CW_UpdateWPM(void)
{
    // Validate WPM. Aligned with CW_Init() bounds (5..100) so the compose-mode
    // WPM table and the keyer agree on the legal range. An uninitialised
    // CW_KEY_WPM (0 or 0xFF) falls back to the platform default.
    uint32_t wpm = gEeprom.CW_KEY_WPM;
    if (wpm < 5 || wpm > 100)
        wpm = CW_DEFAULT_WPM;

    const uint32_t dit_ticks = TICKS_PER_MINUTE / (wpm * DITS_PER_WORD);

    s_dit_count = (uint16_t)dit_ticks;
    s_dah_count = (uint16_t)(3U * dit_ticks);
    s_gap_count = (uint16_t)dit_ticks;
    s_char_gap_count = (uint16_t)(3U * dit_ticks);
    s_word_gap_count = (uint16_t)(7U * dit_ticks);
}

static void CW_KeyerInit(void)
{
    const uint8_t key_input_mode = gEeprom.CW_KEY_INPUT;

    CW_UpdateWPM();
    CW_KeyerResetRuntime();
    s_cfg_dirty = false;

    if (s_enable_keyer && s_last_key_input_mode == key_input_mode) {
        return;
    }

    bool uses_port_ground = (key_input_mode & CW_KEY_FLAG_PORT_GROUND) != 0;
    bool uses_port_ring   = (key_input_mode & CW_KEY_FLAG_PORT_RING) != 0;
    bool uses_adc         = (key_input_mode & CW_KEY_FLAG_ADC) != 0;

    CW_ConfigurePortRing(uses_port_ring);
    CW_ConfigureADCforCECPaddles(uses_adc);
    if (!uses_adc && uses_port_ground)
        CW_ConfigurePortGround(true);

    gCW_KeyerUsingSD1 = (key_input_mode & CW_KEY_FLAG_SIDE1) != 0;
    s_last_key_input_mode = key_input_mode;
    s_enable_keyer = true;
    gCW_KeyerManagesPtt = true;
}

void CW_StartMacroPlayback(uint8_t macroIndex, bool repeat)
{
    if (gCW_Recording || gCW_PlaybackActive) return;

    memset(s_playback_buf, 0, sizeof(s_playback_buf));
    CW_LoadMacro(macroIndex, s_playback_buf, sizeof(s_playback_buf));
    s_playback_buf_len = (uint16_t)strlen(s_playback_buf);
    s_playback_pos = 0;
    gCW_PlaybackMacroIndex = macroIndex;
    s_play_elem_index = 0;
    s_play_char_len = 0;
    s_play_char_pattern = 0;

    gCW_PlaybackRepeat = repeat;
    gCW_MessageRepeatCountdown_500ms = 0;

    CW_ClearTxDisplay();
    s_play_space_pending = false;
    s_pb_state = PB_STATE_INTER_CHAR_GAP;
    s_elem_start_count = gGlobalSysTickCounter;
    gCW_PlaybackActive = (s_playback_buf_len > 0);
}

void CW_StopPlayback(void)
{
    gCW_PlaybackActive = false;
    gCW_PlaybackRepeat = false;
    gCW_MessageRepeatCountdown_500ms = 0;
    s_pb_state = PB_STATE_IDLE;
    s_playback_buf_len = 0;
    s_playback_pos = 0;
}

CW_Action_t CW_PlaybackHandleState(void)
{
    if (!gCW_PlaybackActive) return CW_ACTION_NONE;

    const uint16_t cur_count = gGlobalSysTickCounter;
    CW_Input in = {0};

    CW_ReadKeys(&in);
    if (in.dit || in.dah) {
        CW_StopPlayback();
        return CW_ACTION_NONE;
    }

    switch (s_pb_state) {
    case PB_STATE_ACTIVE_ELEMENT: {
        const uint16_t target = s_active_is_dit ? s_dit_count : s_dah_count;
        const uint16_t elapsed = (uint16_t)(cur_count - s_elem_start_count);
        if (elapsed < target) {
            return CW_ACTION_CARRIER_HOLD_ON;
        } else {
            s_elem_start_count = cur_count;
            s_pb_state = PB_STATE_INTER_ELEMENT_GAP;
            return CW_ACTION_CARRIER_OFF;
        }
    }

    case PB_STATE_INTER_ELEMENT_GAP: {
        const uint16_t elapsed = (uint16_t)(cur_count - s_elem_start_count);
        if (elapsed >= s_gap_count) {
            s_play_elem_index++;
            if (s_play_elem_index < s_play_char_len) {
                bool is_dit = (((s_play_char_pattern >> s_play_elem_index) & 1) == 0);
                s_active_is_dit = is_dit;
                s_elem_start_count = cur_count;
                s_pb_state = PB_STATE_ACTIVE_ELEMENT;
                return CW_ACTION_CARRIER_ON;
            } else {
                s_pb_state = PB_STATE_INTER_CHAR_GAP;
                s_elem_start_count = cur_count;
                return CW_ACTION_NONE;
            }
        }
        return CW_ACTION_NONE;
    }

    case PB_STATE_INTER_CHAR_GAP: {
        const uint16_t elapsed = (uint16_t)(cur_count - s_elem_start_count);
        if (elapsed < s_char_gap_count) {
            return CW_ACTION_NONE;
        }
        if (s_playback_pos >= s_playback_buf_len) {
            if (gCW_PlaybackRepeat && gEeprom.CW_MESSAGE_REPEAT_DELAY > 0) {
                gCW_MessageRepeatCountdown_500ms = gEeprom.CW_MESSAGE_REPEAT_DELAY * 2;
                gCW_PlaybackActive = false;
                s_pb_state = PB_STATE_IDLE;
            } else {
                CW_StopPlayback();
            }
            return CW_ACTION_NONE;
        }
        char ch = s_playback_buf[s_playback_pos++];
        if (ch == ' ') {
            s_play_space_pending = true;
            s_pb_state = PB_STATE_INTER_WORD_GAP;
            s_elem_start_count = cur_count;
            return CW_ACTION_NONE;
        }
        CW_AddToTxDisplay(ch, s_play_space_pending);
        s_play_space_pending = false;

        uint8_t patt = 0, len = 0;
        if (!CW_GetMorseForChar(ch, &patt, &len)) {
            s_pb_state = PB_STATE_INTER_CHAR_GAP;
            s_elem_start_count = cur_count;
            return CW_ACTION_NONE;
        }
        s_play_char_pattern = patt;
        s_play_char_len = len;
        s_play_elem_index = 0;
        bool is_dit = (((s_play_char_pattern >> s_play_elem_index) & 1) == 0);
        s_active_is_dit = is_dit;
        s_elem_start_count = cur_count;
        s_pb_state = PB_STATE_ACTIVE_ELEMENT;
        return CW_ACTION_CARRIER_ON;
    }

    case PB_STATE_INTER_WORD_GAP: {
        const uint16_t elapsed = (uint16_t)(cur_count - s_elem_start_count);
        if (elapsed >= s_word_gap_count) {
            s_pb_state = PB_STATE_INTER_CHAR_GAP;
            s_elem_start_count = cur_count;
        }
        return CW_ACTION_NONE;
    }

    case PB_STATE_IDLE:
    default:
        return CW_ACTION_NONE;
    }
}

void CW_PlaybackIndicatorDeadline(void)
{
    static uint8_t s_tick_count_10ms = 0;
    const uint8_t TARGET_TICKS = 25;

    if (gCW_PlaybackActive || gCW_MessageRepeatCountdown_500ms > 0) {
        if (++s_tick_count_10ms >= TARGET_TICKS) {
            s_tick_count_10ms = 0;
            gCW_PlayIndicatorOn = !gCW_PlayIndicatorOn;
            gUpdateDisplay = true;
        }
    } else {
        s_tick_count_10ms = 0;
        if (gCW_PlayIndicatorOn) {
            gCW_PlayIndicatorOn = false;
            gUpdateDisplay = true;
        }
    }
}

bool CW_CheckKeyerInputs(uint8_t new_mode)
{
    CW_KeyerDeinit();

    bool uses_port_ground = (new_mode & CW_KEY_FLAG_PORT_GROUND) != 0;
    bool uses_port_ring   = (new_mode & CW_KEY_FLAG_PORT_RING) != 0;
    bool uses_adc         = (new_mode & CW_KEY_FLAG_ADC) != 0;

    if ((new_mode & CW_KEY_FLAG_NO_KEYER) && !uses_port_ground && !uses_adc) {
        return true;
    }

    if (!uses_port_ground && !uses_port_ring && !uses_adc) {
        return true;
    }

    if (uses_port_ground)
        CW_ConfigurePortGround(true);
    if (uses_port_ring)
        CW_ConfigurePortRing(true);
    if (uses_adc)
        CW_ConfigureADCforCECPaddles(true);

    SYSTEM_DelayMs(50);

    int stuck_count = 0;
    bool any_stuck = false;

    for (int i = 0; i < 20; i++) {
        bool dit = false, dah = false;
        CW_ReadKeysForMode(new_mode, &dit, &dah);

        if (dit || dah) {
            stuck_count++;
            if (stuck_count > 2) {
                any_stuck = true;
                break;
            }
        }

        SYSTEM_DelayMs(10);
    }

    CW_KeyerDeinit();

    return !any_stuck;
}

CW_Action_t ptt_action(void)
{
    CW_Action_t action = CW_ACTION_NONE;
    bool ptt = false;

    if (gEeprom.CW_KEY_INPUT & CW_KEY_FLAG_ADC) {
        CW_ReadADCkeys(&ptt, &ptt);
    } else {
        ptt = GPIO_IsPttPressed();
    }

    if (ptt && !s_last_handkey_ptt) {
        action = CW_ACTION_CARRIER_ON;
    } else if (!ptt && s_last_handkey_ptt) {
        action = CW_ACTION_CARRIER_OFF;
    } else if (ptt && s_last_handkey_ptt) {
        action = CW_ACTION_CARRIER_HOLD_ON;
    }

    s_last_handkey_ptt = ptt;
    return action;
}

static CW_Action_t CW_HandleBugState(void)
{
    CW_Input in = {0};
    CW_ReadKeys(&in);
    const uint16_t now = gGlobalSysTickCounter;

    switch (s_bug_state) {
    case BUG_STATE_IDLE:
        if (in.dah) {
            s_bug_state = BUG_STATE_DAH_HOLD;
            return CW_ACTION_CARRIER_ON;
        }
        if (in.dit) {
            s_bug_phase_start = now;
            // When the speaker is disabled, add a small timing extension (20 ms)
            // to give the operator a slightly more relaxed feel for the rhythm.
            // This is intentional for operational comfort but technically exceeds
            // strict ITU WPM timing. Make configurable if strict timing is required.
            s_elem_deadline_extra_ms = gEnableSpeaker ? 0 : (CW_ELEM_DEADLINE_EXTRA_MS / 10);
            s_bug_state = BUG_STATE_DIT_ELEMENT;
            return CW_ACTION_CARRIER_ON;
        }
        return CW_ACTION_NONE;

    case BUG_STATE_DIT_ELEMENT:
        if ((uint16_t)(now - s_bug_phase_start) >= (uint16_t)(s_dit_count + s_elem_deadline_extra_ms)) {
            CW_EncoderProcessElement(CW_ELEMENT_DIT);
            s_elem_deadline_extra_ms = 0;
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DIT_GAP;
            return CW_ACTION_CARRIER_OFF;
        }
        return CW_ACTION_CARRIER_HOLD_ON;

    case BUG_STATE_DIT_GAP:
        if (in.dah) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DAH_HOLD;
            return CW_ACTION_CARRIER_ON;
        }
        if (!in.dit) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_CHAR_GAP;
            return CW_ACTION_NONE;
        }
        if ((uint16_t)(now - s_bug_phase_start) >= s_gap_count) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DIT_ELEMENT;
            return CW_ACTION_CARRIER_ON;
        }
        return CW_ACTION_NONE;

    case BUG_STATE_DAH_HOLD:
        if (in.dah) {
            return CW_ACTION_CARRIER_HOLD_ON;
        }
        if ((uint16_t)(now - s_bug_phase_start) >= s_gap_count) {
            CW_EncoderProcessElement(CW_ELEMENT_DAH);
        }
        s_bug_phase_start = now;
        s_bug_state = BUG_STATE_DIT_GAP;
        return CW_ACTION_CARRIER_OFF;

    case BUG_STATE_CHAR_GAP:
        if (in.dah) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DAH_HOLD;
            return CW_ACTION_CARRIER_ON;
        }
        if (in.dit) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DIT_ELEMENT;
            return CW_ACTION_CARRIER_ON;
        }
        if ((uint16_t)(now - s_bug_phase_start) >= s_char_gap_count) {
            CW_EncoderProcessElement(CW_ELEMENT_INTER_CHAR_SPACE);
            s_bug_state = BUG_STATE_WORD_GAP;
        }
        return CW_ACTION_NONE;

    case BUG_STATE_WORD_GAP:
        if (in.dah) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DAH_HOLD;
            return CW_ACTION_CARRIER_ON;
        }
        if (in.dit) {
            s_bug_phase_start = now;
            s_bug_state = BUG_STATE_DIT_ELEMENT;
            return CW_ACTION_CARRIER_ON;
        }
        if ((uint16_t)(now - s_bug_phase_start) >= s_word_gap_count) {
            CW_EncoderProcessElement(CW_ELEMENT_INTER_WORD_SPACE);
            s_bug_state = BUG_STATE_IDLE;
        }
        return CW_ACTION_NONE;
    }

    return CW_ACTION_NONE;
}

CW_Action_t CW_HandleState(void)
{
    CW_Action_t action = CW_ACTION_NONE;
    CW_Input in = {0};

    if (s_cfg_dirty && s_KeyerFSMState == CWK_STATE_IDLE && s_bug_state == BUG_STATE_IDLE) {
        CW_KeyerInit();
        return CW_ACTION_NONE;
    } else if (s_KeyerFSMState == CWK_STATE_EMIT_NONE || !s_enable_keyer) {
        s_KeyerFSMState = CWK_STATE_IDLE;
        return CW_ACTION_NONE;
    }

    const uint16_t cur_count = gGlobalSysTickCounter;

    if (gEeprom.CW_KEY_INPUT & CW_KEY_FLAG_NO_KEYER) {
        return ptt_action();
    }

    if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_BUG) {
        return CW_HandleBugState();
    }

    switch (s_KeyerFSMState) {
    case CWK_STATE_IDLE:
        CW_ReadKeys(&in);
        if (in.dit || in.dah) {
            if (in.dit && !in.dah) {
                s_active_is_dit = true;
            } else if (in.dah && !in.dit) {
                s_active_is_dit = false;
            } else if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_ULTIMATIC) {
                s_active_is_dit = !in.last_is_dah;
            } else {
                s_active_is_dit = !s_active_is_dit;
            }

            s_pending_alternate = false;
            s_elem_start_count = cur_count;
            s_elem_deadline_extra_ms = gEnableSpeaker ? 0 : (CW_ELEM_DEADLINE_EXTRA_MS / 10);
            s_KeyerFSMState = CWK_STATE_ACTIVE_ELEMENT;
            action = CW_ACTION_CARRIER_ON;
        }
        break;

    case CWK_STATE_ACTIVE_ELEMENT: {
        const uint16_t target = s_active_is_dit ? s_dit_count : s_dah_count;
        const uint16_t elapsed_elem = (uint16_t)(cur_count - s_elem_start_count);

        if (!s_pending_alternate) {
            CW_ReadKeys(&in);

            if (gEeprom.CW_KEYER_MODE == CW_IAMBIC_MODE_A) {
                if (s_active_is_dit && in.dah_rise) {
                    s_pending_alternate = true;
                } else if (!s_active_is_dit && in.dit_rise) {
                    s_pending_alternate = true;
                }
            } else if (gEeprom.CW_KEYER_MODE == CW_IAMBIC_MODE_B) {
                if ((!s_active_is_dit) && (elapsed_elem < s_dit_count)) {
                    if (in.dit_rise) {
                        s_pending_alternate = true;
                    }
                } else {
                    if (s_active_is_dit && in.dah) {
                        s_pending_alternate = true;
                    } else if (!s_active_is_dit && in.dit) {
                        s_pending_alternate = true;
                    }
                }
            }
        }

        if (elapsed_elem >= target + s_elem_deadline_extra_ms) {
            s_elem_deadline_extra_ms = 0;
            action = CW_ACTION_CARRIER_OFF;
            s_elem_start_count = cur_count;
            CW_EncoderProcessElement(s_active_is_dit ? CW_ELEMENT_DIT : CW_ELEMENT_DAH);
            s_KeyerFSMState = CWK_STATE_INTER_ELEMENT_GAP;
        } else {
            action = CW_ACTION_CARRIER_HOLD_ON;
        }
        break;
    }

    case CWK_STATE_INTER_ELEMENT_GAP: {
        const uint16_t elapsed_gap = (uint16_t)(cur_count - s_elem_start_count);

        if (!s_pending_alternate) {
            CW_ReadKeys(&in);

            if (gEeprom.CW_KEYER_MODE != CW_KEYER_MODE_ULTIMATIC) {
                if (s_active_is_dit && in.dah_rise) {
                    s_pending_alternate = true;
                } else if (!s_active_is_dit && in.dit_rise) {
                    s_pending_alternate = true;
                }
            }
        }

        if (elapsed_gap >= s_gap_count) {
            bool next_is_dit = false;
            bool have_next = false;

            if (s_pending_alternate) {
                if (!s_active_is_dit)
                    next_is_dit = true;
                have_next = true;
            } else {
                CW_ReadKeys(&in);

                if (in.dit || in.dah) {
                    if (in.dit && !in.dah) {
                        next_is_dit = true;
                    } else if (in.dah && !in.dit) {
                        next_is_dit = false;
                    } else if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_ULTIMATIC) {
                        next_is_dit = !in.last_is_dah;
                    } else {
                        next_is_dit = !s_active_is_dit;
                    }
                    have_next = true;
                }
            }

            s_pending_alternate = false;

            if (have_next) {
                s_elem_start_count = cur_count;
                s_KeyerFSMState = CWK_STATE_ACTIVE_ELEMENT;
                s_active_is_dit = next_is_dit;
                CW_ReadKeys(&in);
                action = CW_ACTION_CARRIER_ON;
            } else {
                s_KeyerFSMState = CWK_STATE_INTER_CHAR_GAP;
            }
        }
        break;
    }

    case CWK_STATE_INTER_CHAR_GAP: {
        const uint16_t elapsed_gap = (uint16_t)(cur_count - s_elem_start_count);

        if (elapsed_gap < s_char_gap_count) {
            CW_ReadKeys(&in);
            bool have_key = (in.dit || in.dah);

            if (elapsed_gap < s_gap_count + (s_gap_count >> 1)) {
                if (have_key) {
                    if (in.dit && !in.dah) {
                        s_active_is_dit = true;
                    } else if (in.dah && !in.dit) {
                        s_active_is_dit = false;
                    } else if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_ULTIMATIC) {
                        s_active_is_dit = !in.last_is_dah;
                    } else {
                        s_active_is_dit = !s_active_is_dit;
                    }

                    s_elem_start_count = cur_count;
                    s_KeyerFSMState = CWK_STATE_ACTIVE_ELEMENT;
                    action = CW_ACTION_CARRIER_ON;
                }
            } else {
                if (have_key && !s_pending_alternate) {
                    s_pending_alternate = true;
                    if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_ULTIMATIC && in.dit && in.dah) {
                        s_active_is_dit = !in.last_is_dah;
                    } else {
                        s_active_is_dit = in.dit;
                    }
                }
            }
        } else {
            CW_EncoderProcessElement(CW_ELEMENT_INTER_CHAR_SPACE);
            if (s_pending_alternate) {
                s_pending_alternate = false;
                s_elem_start_count = cur_count;
                s_KeyerFSMState = CWK_STATE_ACTIVE_ELEMENT;
                action = CW_ACTION_CARRIER_ON;
            } else {
                s_KeyerFSMState = CWK_STATE_INTER_WORD_GAP;
            }
        }
        break;
    }

    case CWK_STATE_INTER_WORD_GAP: {
        const uint16_t elapsed_gap = (uint16_t)(cur_count - s_elem_start_count);
        CW_ReadKeys(&in);

        if (in.dit || in.dah) {
            if (gEeprom.CW_KEYER_MODE == CW_KEYER_MODE_ULTIMATIC && in.dit && in.dah) {
                s_active_is_dit = !in.last_is_dah;
            } else {
                s_active_is_dit = in.dit;
            }
            s_elem_start_count = cur_count;
            s_KeyerFSMState = CWK_STATE_ACTIVE_ELEMENT;
            action = CW_ACTION_CARRIER_ON;
        } else if (elapsed_gap >= s_word_gap_count) {
            CW_EncoderProcessElement(CW_ELEMENT_INTER_WORD_SPACE);
            s_KeyerFSMState = CWK_STATE_IDLE;
        }
        break;
    }

    default:
        s_KeyerFSMState = CWK_STATE_IDLE;
        break;
    }

    return action;
}
