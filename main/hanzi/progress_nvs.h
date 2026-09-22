#pragma once
#include "esp_err.h"
#include "hanzi_deck.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t progress_nvs_load(hanzi_progress_t *out);
esp_err_t progress_nvs_save(const hanzi_progress_t *in);

#ifdef __cplusplus
}
#endif
