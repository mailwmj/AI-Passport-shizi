#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t index_global; /* 1-based curriculum index */
    uint16_t sample_count;
    const uint8_t *adpcm;
    uint16_t adpcm_len;
} voice_clip_t;

bool voice_pack_init(void);
uint16_t voice_pack_count(void);
bool voice_pack_find(uint16_t index_global_1based, voice_clip_t *out);

#ifdef __cplusplus
}
#endif
