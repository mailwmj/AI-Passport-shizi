#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default kids volume when NVS has no saved value. */
#define SETTINGS_VOLUME_DEFAULT 90
/** Prefer min 10 so kids don't mute forever via long-DOWN. */
#define SETTINGS_VOLUME_MIN 10
#define SETTINGS_VOLUME_MAX 100
#define SETTINGS_VOLUME_STEP 10

esp_err_t settings_nvs_load_volume(uint8_t *out);
esp_err_t settings_nvs_save_volume(uint8_t volume);

#ifdef __cplusplus
}
#endif
