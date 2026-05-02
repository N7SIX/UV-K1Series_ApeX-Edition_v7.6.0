#include <string.h>

#include "driver/py25q16.h"
#include "settings.h"
#include "settings_migration.h"

#define LEGACY_VERSION_NONE      SETTINGS_MIGRATION_NONE
#define LEGACY_VERSION_F4HWN     SETTINGS_MIGRATION_F4HWN
#define LEGACY_VERSION_STOCK     SETTINGS_MIGRATION_STOCK
#define LEGACY_VERSION_EGZUMER   SETTINGS_MIGRATION_EGZUMER
#define LEGACY_VERSION_GENERIC   SETTINGS_MIGRATION_GENERIC

typedef struct {
    uint32_t rx_freq;
    uint32_t tx_freq;
    uint8_t modulation;     // 0=FM, 1=AM
    uint8_t bandwidth;      // 0=wide, 1=narrow
    uint16_t ctcss_dcs_code; // CTCSS/DCS code
    uint8_t power;          // 0-7 power level
    uint8_t step;           // frequency step index
    uint8_t reserved[2];    // padding to 16 bytes
} LegacyChannel_t;

static uint16_t SETTINGS_ImportLegacyChannels(void);
static uint16_t SETTINGS_ImportLegacyChannelNames(void);
static uint16_t SETTINGS_ImportLegacySettings(void);
static uint16_t SETTINGS_ImportStockChannels(void);
static uint16_t SETTINGS_ImportStockSettings(void);
static uint16_t SETTINGS_ImportEgzumerChannels(void);
static uint16_t SETTINGS_ImportEgzumerSettings(void);
static uint16_t SETTINGS_ImportGenericChannels(void);
static uint16_t SETTINGS_ImportGenericSettings(void);

static SETTINGS_MigrationResult_t gMigrationResult = {
    .variant = SETTINGS_MIGRATION_NONE,
    .status = SETTINGS_MIGRATION_STATUS_SKIPPED,
    .channelsImported = 0,
    .channelNamesImported = 0,
    .settingsImported = 0,
    .adapterName = "none"
};

const SETTINGS_MigrationResult_t *SETTINGS_GetMigrationResult(void)
{
    return &gMigrationResult;
}

void SETTINGS_ResetMigrationResult(void)
{
    gMigrationResult.variant = SETTINGS_MIGRATION_NONE;
    gMigrationResult.status = SETTINGS_MIGRATION_STATUS_SKIPPED;
    gMigrationResult.channelsImported = 0;
    gMigrationResult.channelNamesImported = 0;
    gMigrationResult.settingsImported = 0;
    gMigrationResult.adapterName = "none";
}

static bool SETTINGS_IsValidLegacyFrequency(uint32_t frequency)
{
    return frequency >= 10000000u && frequency <= 1000000000u;
}

static uint16_t SETTINGS_CountValidLegacyChannels(uint32_t address, uint16_t sampleCount)
{
    uint8_t data[4];
    uint16_t validCount = 0;

    for (uint16_t ch = 0; ch < sampleCount; ch++) {
        PY25Q16_ReadBuffer(address + (ch * 16), data, 4);
        uint32_t frequency = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

        if (SETTINGS_IsValidLegacyFrequency(frequency)) {
            validCount++;
        }
    }

    return validCount;
}

static bool SETTINGS_IsPlausibleLegacySettings(uint32_t address)
{
    uint8_t data[8];
    PY25Q16_ReadBuffer(address, data, sizeof(data));

    if (data[0] >= 10) {
        return false;
    }
    if (data[1] < 5 || data[1] >= 180) {
        return false;
    }
    if (data[2] >= 255) {
        return false;
    }
    if (data[3] >= 2 || data[4] >= 2 || data[5] >= 10) {
        return false;
    }
    if (data[6] >= 5 || data[7] >= 62) {
        return false;
    }

    return true;
}

static bool SETTINGS_IsLikelyF4HWN(void)
{
    if (!SETTINGS_IsPlausibleLegacySettings(0x00A000)) {
        return false;
    }

    return SETTINGS_CountValidLegacyChannels(0x000000, 20) >= 10;
}

static bool SETTINGS_IsLikelyStock(void)
{
    if (!SETTINGS_IsPlausibleLegacySettings(0x008000)) {
        return false;
    }

    return SETTINGS_CountValidLegacyChannels(0x008000, 20) >= 10;
}

static bool SETTINGS_IsLikelyEgzumer(void)
{
    uint8_t data[4];
    PY25Q16_ReadBuffer(0x01F000, data, 4);

    if (data[0] == 'E' && data[1] == 'G') {
        return SETTINGS_CountValidLegacyChannels(0x000000, 20) >= 10;
    }

    return false;
}

static bool SETTINGS_IsLikelyGeneric(void)
{
    const uint32_t settingsChecks[] = { 0x00A000, 0x008000, 0x00E000 };
    const uint32_t channelChecks[] = { 0x004000, 0x008000, 0x000000 };
    bool plausibleSettings = false;

    for (int i = 0; i < (int)(sizeof(settingsChecks) / sizeof(settingsChecks[0])); i++) {
        if (SETTINGS_IsPlausibleLegacySettings(settingsChecks[i])) {
            plausibleSettings = true;
            break;
        }
    }

    if (!plausibleSettings) {
        return false;
    }

    for (int i = 0; i < (int)(sizeof(channelChecks) / sizeof(channelChecks[0])); i++) {
        if (SETTINGS_CountValidLegacyChannels(channelChecks[i], 20) >= 10) {
            return true;
        }
    }

    return false;
}

static uint8_t SETTINGS_DetectLegacyVersion(void)
{
    if (SETTINGS_IsLikelyEgzumer()) {
        return LEGACY_VERSION_EGZUMER;
    }

    if (SETTINGS_IsLikelyF4HWN()) {
        return LEGACY_VERSION_F4HWN;
    }

    if (SETTINGS_IsLikelyStock()) {
        return LEGACY_VERSION_STOCK;
    }

    if (SETTINGS_IsLikelyGeneric()) {
        return LEGACY_VERSION_GENERIC;
    }

    return LEGACY_VERSION_NONE;
}

static uint16_t SETTINGS_ImportLegacyChannels(void)
{
    uint16_t imported = 0;
    for (uint16_t ch = 0; ch < 200; ch++) {
        LegacyChannel_t legacy;
        PY25Q16_ReadBuffer(ch * 16, &legacy, sizeof(legacy));

        if (legacy.rx_freq < 10000000 || legacy.rx_freq > 1000000000) {
            continue;
        }

        VFO_Info_t vfo = {0};
        vfo.freq_config_RX.Frequency = legacy.rx_freq;
        vfo.freq_config_TX.Frequency = legacy.tx_freq;
        if (legacy.tx_freq < 10000000 || legacy.tx_freq > 1000000000) {
            vfo.freq_config_TX.Frequency = legacy.rx_freq;
        }
        vfo.pRX = &vfo.freq_config_RX;
        vfo.pTX = &vfo.freq_config_TX;
        vfo.Modulation = (legacy.modulation == 1) ? MODULATION_AM : MODULATION_FM;
        vfo.CHANNEL_BANDWIDTH = legacy.bandwidth ? BANDWIDTH_NARROW : BANDWIDTH_WIDE;

        if (legacy.ctcss_dcs_code > 0 && legacy.ctcss_dcs_code < 1000) {
            vfo.freq_config_RX.CodeType = CODE_TYPE_CONTINUOUS_TONE;
            vfo.freq_config_RX.Code = legacy.ctcss_dcs_code;
            vfo.freq_config_TX.CodeType = CODE_TYPE_CONTINUOUS_TONE;
            vfo.freq_config_TX.Code = legacy.ctcss_dcs_code;
        }

        vfo.OUTPUT_POWER = (legacy.power < 8) ? legacy.power : OUTPUT_POWER_MID;
        vfo.StepFrequency = (legacy.step < STEP_N_ELEM) ? gStepFrequencyTable[legacy.step] : 25000;
        vfo.STEP_SETTING = (legacy.step < STEP_N_ELEM) ? legacy.step : STEP_25kHz;
        vfo.TX_OFFSET_FREQUENCY = 0;
        vfo.TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
        vfo.CHANNEL_SAVE = 0;
        vfo.TX_LOCK = 0;
        vfo.TXP_CalculatedSetting = 0;
        vfo.FrequencyReverse = false;
        vfo.SCRAMBLING_TYPE = 0;
        vfo.SCANLIST1_PARTICIPATION = 0;
        vfo.SCANLIST2_PARTICIPATION = 0;
        vfo.SCANLIST3_PARTICIPATION = 0;
        vfo.Band = 0;
#ifdef ENABLE_DTMF_CALLING
        vfo.DTMF_DECODING_ENABLE = 0;
#endif
        vfo.DTMF_PTT_ID_TX_MODE = PTT_ID_OFF;
        vfo.BUSY_CHANNEL_LOCK = 0;
        vfo.Compander = 0;
        memset(vfo.Name, 0, sizeof(vfo.Name));

        SETTINGS_SaveChannel(ch, 0, &vfo, 0);
        imported++;
    }

    return imported;
}

static uint16_t SETTINGS_ImportLegacyChannelNames(void)
{
    uint16_t imported = 0;
    for (uint16_t ch = 0; ch < 200; ch++) {
        char name[11];
        PY25Q16_ReadBuffer(0x004000 + ch * 16, name, 10);
        name[10] = '\0';

        bool valid = true;
        for (int i = 0; i < 10 && name[i]; i++) {
            if (name[i] < 32 || name[i] > 126) {
                valid = false;
                break;
            }
        }

        if (valid && name[0]) {
            SETTINGS_SaveChannelName(ch, name);
            imported++;
        }
    }

    return imported;
}

static uint16_t SETTINGS_ImportLegacySettings(void)
{
    uint16_t imported = 0;
    uint8_t data[16];
    PY25Q16_ReadBuffer(0x00A000, data, 16);

    uint8_t squelch = gEeprom.SQUELCH_LEVEL;
    uint8_t timeout = gEeprom.TX_TIMEOUT_TIMER;
    uint8_t keyLock = gEeprom.KEY_LOCK ? 1 : 0;
    uint8_t voxSwitch = gEeprom.VOX_SWITCH ? 1 : 0;
    uint8_t voxLevel = gEeprom.VOX_LEVEL;
    uint8_t micSensitivity = gEeprom.MIC_SENSITIVITY;
    uint8_t backlightTime = gEeprom.BACKLIGHT_TIME;
    uint8_t dualWatch = gEeprom.DUAL_WATCH;
    uint8_t crossBand = gEeprom.CROSS_BAND_RX_TX;
    uint8_t batterySave = gEeprom.BATTERY_SAVE;
    uint8_t channelDisplayMode = gEeprom.CHANNEL_DISPLAY_MODE;

    if (data[1] < 10) {
        squelch = data[1];
        imported++;
    }

    if (data[2] >= 5 && data[2] < 180) {
        timeout = data[2];
        imported++;
    }

    if (data[3] < 2) { keyLock = data[3]; imported++; }
    if (data[4] < 2) { voxSwitch = data[4]; imported++; }
    if (data[5] < 10) { voxLevel = data[5]; imported++; }
    if (data[6] < 5) { micSensitivity = data[6]; imported++; }
    if (data[7] < 62) { backlightTime = data[7]; imported++; }
    if (data[8] < 3) { dualWatch = data[8]; imported++; }
    if (data[9] < 3) { crossBand = data[9]; imported++; }
    if (data[10] < 6) { batterySave = data[10]; imported++; }
    if (data[11] < 4) { channelDisplayMode = data[11]; imported++; }

    if (imported < 4) {
        return 0;
    }

    gEeprom.SQUELCH_LEVEL = squelch;
    gEeprom.TX_TIMEOUT_TIMER = timeout;
    gEeprom.KEY_LOCK = keyLock;
    gEeprom.VOX_SWITCH = voxSwitch;
    gEeprom.VOX_LEVEL = voxLevel;
    gEeprom.MIC_SENSITIVITY = micSensitivity;
    gEeprom.BACKLIGHT_TIME = backlightTime;
    gEeprom.DUAL_WATCH = dualWatch;
    gEeprom.CROSS_BAND_RX_TX = crossBand;
    gEeprom.BATTERY_SAVE = batterySave;
    gEeprom.CHANNEL_DISPLAY_MODE = channelDisplayMode;

    SETTINGS_SaveSettings();
    return imported;
}

static uint16_t SETTINGS_ImportStockChannels(void)
{
    uint16_t imported = 0;
    for (uint16_t ch = 0; ch < 200; ch++) {
        uint8_t buf[16];
        uint32_t stock_addr = 0x008000 + (ch * 16);
        PY25Q16_ReadBuffer(stock_addr, buf, 16);

        uint32_t rx_freq = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        if (rx_freq < 10000000 || rx_freq > 1000000000) {
            continue;
        }

        uint32_t tx_freq = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
        VFO_Info_t vfo = {0};
        vfo.freq_config_RX.Frequency = rx_freq;
        vfo.freq_config_TX.Frequency = (tx_freq > 10000000) ? tx_freq : rx_freq;
        vfo.pRX = &vfo.freq_config_RX;
        vfo.pTX = &vfo.freq_config_TX;
        vfo.Modulation = MODULATION_FM;
        vfo.CHANNEL_BANDWIDTH = BANDWIDTH_WIDE;
        vfo.OUTPUT_POWER = OUTPUT_POWER_MID;
        vfo.STEP_SETTING = STEP_25kHz;
        vfo.StepFrequency = 25000;
        vfo.TX_OFFSET_FREQUENCY = 0;
        vfo.TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
        vfo.CHANNEL_SAVE = 0;
        vfo.TX_LOCK = 0;
        vfo.FrequencyReverse = false;
        vfo.SCRAMBLING_TYPE = 0;
        vfo.SCANLIST1_PARTICIPATION = 0;
        vfo.SCANLIST2_PARTICIPATION = 0;
        vfo.SCANLIST3_PARTICIPATION = 0;
        vfo.Band = 0;
        vfo.DTMF_PTT_ID_TX_MODE = PTT_ID_OFF;
        vfo.BUSY_CHANNEL_LOCK = 0;
        vfo.Compander = 0;
        memset(vfo.Name, 0, sizeof(vfo.Name));

        SETTINGS_SaveChannel(ch, 0, &vfo, 0);
        imported++;
    }

    return imported;
}

static uint16_t SETTINGS_ImportStockSettings(void)
{
    uint16_t imported = 0;
    uint8_t data[16];
    PY25Q16_ReadBuffer(0x008000, data, 8);

    uint8_t squelch = gEeprom.SQUELCH_LEVEL;
    uint8_t timeout = gEeprom.TX_TIMEOUT_TIMER;
    uint8_t keyLock = gEeprom.KEY_LOCK ? 1 : 0;
    uint8_t voxSwitch = gEeprom.VOX_SWITCH ? 1 : 0;
    uint8_t voxLevel = gEeprom.VOX_LEVEL;
    uint8_t micSensitivity = gEeprom.MIC_SENSITIVITY;
    uint8_t backlightTime = gEeprom.BACKLIGHT_TIME;
    uint8_t dualWatch = gEeprom.DUAL_WATCH;
    uint8_t crossBand = gEeprom.CROSS_BAND_RX_TX;
    uint8_t batterySave = gEeprom.BATTERY_SAVE;
    uint8_t channelDisplayMode = gEeprom.CHANNEL_DISPLAY_MODE;

    if (data[0] < 10) {
        squelch = data[0];
        imported++;
    }

    if (data[1] >= 5 && data[1] < 180) {
        timeout = data[1];
        imported++;
    }

    if (data[2] < 2) { keyLock = data[2]; imported++; }
    if (data[3] < 2) { voxSwitch = data[3]; imported++; }
    if (data[4] < 10) { voxLevel = data[4]; imported++; }
    if (data[5] < 5) { micSensitivity = data[5]; imported++; }
    if (data[6] < 62) { backlightTime = data[6]; imported++; }
    if (data[7] < 3) { dualWatch = data[7]; imported++; }

    PY25Q16_ReadBuffer(0x008008, data, 8);
    if (data[0] < 3) { crossBand = data[0]; imported++; }
    if (data[1] < 6) { batterySave = data[1]; imported++; }
    if (data[2] < 4) { channelDisplayMode = data[2]; imported++; }

    if (imported < 4) {
        return 0;
    }

    gEeprom.SQUELCH_LEVEL = squelch;
    gEeprom.TX_TIMEOUT_TIMER = timeout;
    gEeprom.KEY_LOCK = keyLock;
    gEeprom.VOX_SWITCH = voxSwitch;
    gEeprom.VOX_LEVEL = voxLevel;
    gEeprom.MIC_SENSITIVITY = micSensitivity;
    gEeprom.BACKLIGHT_TIME = backlightTime;
    gEeprom.DUAL_WATCH = dualWatch;
    gEeprom.CROSS_BAND_RX_TX = crossBand;
    gEeprom.BATTERY_SAVE = batterySave;
    gEeprom.CHANNEL_DISPLAY_MODE = channelDisplayMode;

    SETTINGS_SaveSettings();
    return imported;
}

static uint16_t SETTINGS_ImportEgzumerChannels(void)
{
    return SETTINGS_ImportLegacyChannels();
}

static uint16_t SETTINGS_ImportEgzumerSettings(void)
{
    uint16_t imported = SETTINGS_ImportLegacySettings();
    uint8_t data[8];
    PY25Q16_ReadBuffer(0x01F000 + 0x10, data, 8);
    if (data[0] < 2) {
        // Egzumer-specific extension area; preserve known-safe defaults.
    }

    return imported;
}

static uint16_t SETTINGS_ImportGenericChannels(void)
{
    const uint32_t addr_attempts[] = {
        0x004000,
        0x008000,
        0x000000,
    };
    uint16_t imported = 0;

    for (int attempt = 0; attempt < 3 && !imported; attempt++) {
        uint32_t base_addr = addr_attempts[attempt];
        int valid_count = 0;

        for (uint16_t ch = 0; ch < 200; ch++) {
            uint8_t buf[16];
            PY25Q16_ReadBuffer(base_addr + (ch * 16), buf, 16);
            uint32_t freq = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
            if (freq >= 10000000 && freq <= 1000000000) {
                valid_count++;
            }
        }

        if (valid_count >= 10) {
            for (uint16_t ch = 0; ch < 200; ch++) {
                uint8_t buf[16];
                PY25Q16_ReadBuffer(base_addr + (ch * 16), buf, 16);
                uint32_t rx_freq = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
                if (rx_freq < 10000000 || rx_freq > 1000000000) {
                    continue;
                }

                VFO_Info_t vfo = {0};
                vfo.freq_config_RX.Frequency = rx_freq;
                vfo.freq_config_TX.Frequency = rx_freq;
                vfo.pRX = &vfo.freq_config_RX;
                vfo.pTX = &vfo.freq_config_TX;
                vfo.Modulation = MODULATION_FM;
                vfo.CHANNEL_BANDWIDTH = BANDWIDTH_WIDE;
                vfo.OUTPUT_POWER = OUTPUT_POWER_MID;
                vfo.STEP_SETTING = STEP_25kHz;
                vfo.StepFrequency = 25000;
                vfo.TX_OFFSET_FREQUENCY = 0;
                vfo.TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
                vfo.CHANNEL_SAVE = 0;
                vfo.TX_LOCK = 0;
                vfo.FrequencyReverse = false;
                vfo.SCRAMBLING_TYPE = 0;
                vfo.SCANLIST1_PARTICIPATION = 0;
                vfo.SCANLIST2_PARTICIPATION = 0;
                vfo.SCANLIST3_PARTICIPATION = 0;
                vfo.Band = 0;
                vfo.DTMF_PTT_ID_TX_MODE = PTT_ID_OFF;
                vfo.BUSY_CHANNEL_LOCK = 0;
                vfo.Compander = 0;
                memset(vfo.Name, 0, sizeof(vfo.Name));

                SETTINGS_SaveChannel(ch, 0, &vfo, 0);
                imported++;
            }
        }
    }
    return imported;
}

static uint16_t SETTINGS_ImportGenericSettings(void)
{
    uint16_t imported = 0;
    uint8_t data[16];
    const uint32_t addr_attempts[] = {
        0x00A000,
        0x008000,
        0x00E000,
    };

    for (int attempt = 0; attempt < 3; attempt++) {
        if (!SETTINGS_IsPlausibleLegacySettings(addr_attempts[attempt])) {
            continue;
        }

        PY25Q16_ReadBuffer(addr_attempts[attempt], data, 8);
        uint8_t squelch = gEeprom.SQUELCH_LEVEL;
        uint8_t timeout = gEeprom.TX_TIMEOUT_TIMER;
        uint8_t keyLock = gEeprom.KEY_LOCK ? 1 : 0;
        uint8_t voxSwitch = gEeprom.VOX_SWITCH ? 1 : 0;
        uint8_t voxLevel = gEeprom.VOX_LEVEL;
        uint8_t micSensitivity = gEeprom.MIC_SENSITIVITY;
        uint8_t backlightTime = gEeprom.BACKLIGHT_TIME;
        uint8_t dualWatch = gEeprom.DUAL_WATCH;

    if (data[0] < 10) { squelch = data[0]; imported++; }
    if (data[1] >= 5 && data[1] < 180) { timeout = data[1]; imported++; }

        gEeprom.SQUELCH_LEVEL = squelch;
        gEeprom.TX_TIMEOUT_TIMER = timeout;
        gEeprom.KEY_LOCK = keyLock;
        gEeprom.VOX_SWITCH = voxSwitch;
        gEeprom.VOX_LEVEL = voxLevel;
        gEeprom.MIC_SENSITIVITY = micSensitivity;
        gEeprom.BACKLIGHT_TIME = backlightTime;
        gEeprom.DUAL_WATCH = dualWatch;

        SETTINGS_SaveSettings();
        return imported;
    }
    return 0;
}

bool SETTINGS_ImportLegacyFirmware(void)
{
    SETTINGS_ResetMigrationResult();
    uint8_t legacy_version = SETTINGS_DetectLegacyVersion();
    gMigrationResult.variant = legacy_version;

    switch (legacy_version) {
        case LEGACY_VERSION_F4HWN: {
            gMigrationResult.adapterName = "F4HWN";
            gMigrationResult.channelsImported = SETTINGS_ImportLegacyChannels();
            gMigrationResult.channelNamesImported = SETTINGS_ImportLegacyChannelNames();
            gMigrationResult.settingsImported = SETTINGS_ImportLegacySettings();
            break;
        }

        case LEGACY_VERSION_STOCK: {
            gMigrationResult.adapterName = "STOCK";
            gMigrationResult.channelsImported = SETTINGS_ImportStockChannels();
            gMigrationResult.channelNamesImported = 0;
            gMigrationResult.settingsImported = SETTINGS_ImportStockSettings();
            break;
        }

        case LEGACY_VERSION_EGZUMER: {
            gMigrationResult.adapterName = "EGZUMER";
            gMigrationResult.channelsImported = SETTINGS_ImportEgzumerChannels();
            gMigrationResult.channelNamesImported = 0;
            gMigrationResult.settingsImported = SETTINGS_ImportEgzumerSettings();
            break;
        }

        case LEGACY_VERSION_GENERIC: {
            gMigrationResult.adapterName = "GENERIC";
            gMigrationResult.channelsImported = SETTINGS_ImportGenericChannels();
            gMigrationResult.channelNamesImported = 0;
            gMigrationResult.settingsImported = SETTINGS_ImportGenericSettings();
            break;
        }

        default:
            gMigrationResult.status = SETTINGS_MIGRATION_STATUS_SKIPPED;
            gMigrationResult.adapterName = "none";
            return false;
    }

    if (gMigrationResult.channelsImported == 0 && gMigrationResult.settingsImported == 0) {
        gMigrationResult.status = SETTINGS_MIGRATION_STATUS_ERROR;
        return false;
    }

    gMigrationResult.status = SETTINGS_MIGRATION_STATUS_SUCCESS;
    return true;
}
