#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode custom IMA-ADPCM blob (see scripts/gen_voice_pack.py) into int16 PCM.
 * Returns number of samples written, or 0 on error. */
size_t voice_adpcm_decode(const uint8_t *adpcm, size_t adpcm_len,
                          int16_t *pcm_out, size_t pcm_cap_samples);

#ifdef __cplusplus
}
#endif
