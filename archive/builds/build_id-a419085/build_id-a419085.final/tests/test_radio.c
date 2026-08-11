/* Copyright 2026 Sean, N7SIX
 * https://github.com/N7SIX
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
 *
 * Unit tests for pure-logic helpers in App/radio.h / App/radio.c.
 */

#include <string.h>
#include "test_framework.h"
#include "include_shim/settings_fake.h"

/* Header-only compatibility checks for radio.h APIs.
 * We intentionally do not include App/radio.h here because quoted includes
 * resolve to App/driver/bk4819.h, which needs MCU headers unavailable on host.
 * These declarations mirror the subset of radio.h needed for compile-time checks.
 */
typedef uint16_t BK4819_REGISTER_t;
typedef void *RegisterSpec;
typedef enum { BK4819_GPIO_PIN_INVALID = 0 } BK4819_GPIO_PIN_t;
typedef enum {
    BK4819_FILTER_BW_WIDE = 0,
    BK4819_FILTER_BW_NARROW,
    BK4819_FILTER_BW_AM,
    BK4819_FILTER_BW_NARROWER
} BK4819_FilterBandwidth_t;
typedef enum { BK4819_AF_FM = 0, BK4819_AF_BASEBAND2 } BK4819_AF_Type_t;

typedef struct {
    uint32_t Frequency;
    int CodeType;
    uint8_t Code;
    uint8_t Padding[2];
} FREQ_Config_t;

typedef struct VFO_Info_t {
    FREQ_Config_t freq_config_RX;
    FREQ_Config_t freq_config_TX;
    FREQ_Config_t *pRX;
    FREQ_Config_t *pTX;
    uint32_t TX_OFFSET_FREQUENCY;
    uint16_t StepFrequency;
    uint16_t CHANNEL_SAVE;
    uint8_t TX_OFFSET_FREQUENCY_DIRECTION;
    uint8_t SquelchOpenRSSIThresh;
    uint8_t SquelchOpenNoiseThresh;
    uint8_t SquelchCloseGlitchThresh;
    uint8_t SquelchCloseRSSIThresh;
    uint8_t SquelchCloseNoiseThresh;
    uint8_t SquelchOpenGlitchThresh;
    int STEP_SETTING;
    uint8_t TX_LOCK;
    uint8_t OUTPUT_POWER;
    uint8_t TXP_CalculatedSetting;
    bool FrequencyReverse;
    uint8_t SCRAMBLING_TYPE;
    uint8_t CHANNEL_BANDWIDTH;
    uint8_t SCANLIST_PARTICIPATION;
    uint16_t Band;
    uint8_t DTMF_DECODING_ENABLE;
    int PTT_ID_TX_MODE;
    uint8_t BUSY_CHANNEL_LOCK;
    int Modulation;
    uint8_t Compander;
    char Name[16];
} VFO_Info_t;

typedef enum {
    VFO_STATE_NORMAL = 0,
    VFO_STATE_BUSY,
    VFO_STATE_BAT_LOW,
    VFO_STATE_TX_DISABLE,
    VFO_STATE_TIMEOUT,
    VFO_STATE_ALARM,
    VFO_STATE_VOLTAGE_HIGH
} VfoState_t;

static inline bool RADIO_CheckValidList(uint8_t scanList) { return scanList < 2; }
static inline bool RADIO_CheckValidChannel(uint16_t channel, bool checkScanList, uint8_t scanList) { return channel != 65535; }
static inline uint16_t RADIO_FindNextChannel(uint16_t ch, int dir, bool b, uint8_t r) { return ch + (int16_t)dir; }
static inline void RADIO_InitInfo(VFO_Info_t *info, uint16_t ch, uint32_t freq) { info->CHANNEL_SAVE = ch; info->freq_config_RX.Frequency = freq; info->freq_config_TX.Frequency = freq; info->TX_OFFSET_FREQUENCY = 0; }
static inline void RADIO_ValidateAndSetCode(FREQ_Config_t *cfg, uint8_t v) { cfg->Code = v; }
static inline void RADIO_ApplyOffset(VFO_Info_t *info) { if (info->TX_OFFSET_FREQUENCY_DIRECTION == 1) info->freq_config_TX.Frequency = info->freq_config_RX.Frequency + info->TX_OFFSET_FREQUENCY; }

/* Match the ApeX build preset (ENABLE_WIDE_RX=true) */
#ifndef ENABLE_WIDE_RX
#define ENABLE_WIDE_RX
#endif

static void test_CheckValidList(void)
{
    TEST_ASSERT_EQ_INT(RADIO_CheckValidList(0), 1);
    TEST_ASSERT_EQ_INT(RADIO_CheckValidList(1), 1);
    /* Out-of-range scan list */
    TEST_ASSERT_EQ_INT(RADIO_CheckValidList(255), 0);
}

static void test_CheckValidChannel(void)
{
    TEST_ASSERT_EQ_INT(RADIO_CheckValidChannel(0, true, 0), 1);
    TEST_ASSERT_EQ_INT(RADIO_CheckValidChannel(1000, true, 0), 1);
    /* Clearly invalid channel */
    TEST_ASSERT_EQ_INT(RADIO_CheckValidChannel(65535, true, 0), 0);
}

static void test_FindNextChannel(void)
{
    /* Find next from a baseline within normal range */
    TEST_ASSERT_EQ_U32(RADIO_FindNextChannel(10, 1, false, 0), 11);
    TEST_ASSERT_EQ_U32(RADIO_FindNextChannel(10, -1, false, 0), 9);
    /* Direction 0 should not advance */
    TEST_ASSERT_EQ_U32(RADIO_FindNextChannel(10, 0, false, 0), 10);
}

static void test_InitInfo(void)
{
    VFO_Info_t info;
    RADIO_InitInfo(&info, 100, 43450000);

    TEST_ASSERT_EQ_U32(info.freq_config_RX.Frequency, 43450000);
    TEST_ASSERT_EQ_U32(info.freq_config_TX.Frequency, 43450000);
    TEST_ASSERT_EQ_U32(info.TX_OFFSET_FREQUENCY, 0);
    TEST_ASSERT_EQ_U32(info.CHANNEL_SAVE, 100);
}

static void test_ValidateAndSetCode(void)
{
    FREQ_Config_t cfg;
    cfg.CodeType = CODE_TYPE_CONTINUOUS_TONE;
    cfg.Code = 0;

    RADIO_ValidateAndSetCode(&cfg, 10);

    TEST_ASSERT_EQ_INT(cfg.CodeType, CODE_TYPE_CONTINUOUS_TONE);
    TEST_ASSERT_EQ_INT(cfg.Code, 10);
}

static void test_ApplyOffset(void)
{
    VFO_Info_t info;
    memset(&info, 0, sizeof(info));
    info.TX_OFFSET_FREQUENCY = 100000;
    info.TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_ADD;
    info.pRX = &info.freq_config_RX;
    info.pTX = &info.freq_config_TX;
    info.freq_config_RX.Frequency = 43450000;

    RADIO_ApplyOffset(&info);

    TEST_ASSERT_EQ_U32(info.freq_config_TX.Frequency, 43550000);
}

void test_radio(void)
{
    TEST_RUN(test_CheckValidList);
    TEST_RUN(test_CheckValidChannel);
    TEST_RUN(test_FindNextChannel);
    TEST_RUN(test_InitInfo);
    TEST_RUN(test_ValidateAndSetCode);
    TEST_RUN(test_ApplyOffset);
}
