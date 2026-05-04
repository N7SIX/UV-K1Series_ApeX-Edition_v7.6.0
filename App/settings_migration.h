#ifndef SETTINGS_MIGRATION_H
#define SETTINGS_MIGRATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_MIGRATION_NONE = 0,
    SETTINGS_MIGRATION_F4HWN,
    SETTINGS_MIGRATION_STOCK,
    SETTINGS_MIGRATION_EGZUMER,
    SETTINGS_MIGRATION_GENERIC,
} SETTINGS_MigrationVariant_t;

typedef enum {
    SETTINGS_MIGRATION_STATUS_SKIPPED = 0,
    SETTINGS_MIGRATION_STATUS_SUCCESS,
    SETTINGS_MIGRATION_STATUS_ERROR,
} SETTINGS_MigrationStatus_t;

typedef struct {
    SETTINGS_MigrationVariant_t variant;
    SETTINGS_MigrationStatus_t status;
    uint16_t channelsImported;
    uint16_t channelNamesImported;
    uint16_t settingsImported;
    const char *adapterName;
} SETTINGS_MigrationResult_t;

bool SETTINGS_ImportLegacyFirmware(void);
const SETTINGS_MigrationResult_t *SETTINGS_GetMigrationResult(void);
void SETTINGS_ResetMigrationResult(void);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_MIGRATION_H
