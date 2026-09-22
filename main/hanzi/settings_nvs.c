#include "settings_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "settings_nvs";
/* Same kids namespace as progress; orthogonal key "vol". */
static const char *NS = "kids_hanzi";
static const char *KEY_VOL = "vol";

static uint8_t clamp_volume(uint8_t v)
{
    if (v < SETTINGS_VOLUME_MIN) return SETTINGS_VOLUME_MIN;
    if (v > SETTINGS_VOLUME_MAX) return SETTINGS_VOLUME_MAX;
    return v;
}

esp_err_t settings_nvs_load_volume(uint8_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = SETTINGS_VOLUME_DEFAULT;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    uint8_t vol = SETTINGS_VOLUME_DEFAULT;
    if (nvs_get_u8(h, KEY_VOL, &vol) == ESP_OK) {
        *out = clamp_volume(vol);
    }
    nvs_close(h);
    ESP_LOGI(TAG, "volume=%u", (unsigned)*out);
    return ESP_OK;
}

esp_err_t settings_nvs_save_volume(uint8_t volume)
{
    volume = clamp_volume(volume);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, KEY_VOL, volume);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
