#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_worker_start(void);
void audio_worker_stop(void);
void audio_worker_play_hanzi(uint16_t index_global_1based);
void audio_worker_play_tone_fallback(void);

/** Runtime playback volume 10..100 (clamped). Applied on next/ongoing play. */
void audio_worker_set_volume(uint8_t percent);
uint8_t audio_worker_get_volume(void);

#ifdef __cplusplus
}
#endif
