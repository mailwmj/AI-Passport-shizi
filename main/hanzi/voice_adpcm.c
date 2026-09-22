#include "voice_adpcm.h"

static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};
static const int8_t IMA_INDEX[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

size_t voice_adpcm_decode(const uint8_t *adpcm, size_t adpcm_len,
                          int16_t *pcm_out, size_t pcm_cap_samples)
{
    if (!adpcm || adpcm_len < 4 || !pcm_out || pcm_cap_samples == 0) return 0;
    int32_t pred = (int16_t)(adpcm[0] | (adpcm[1] << 8));
    int index = adpcm[2];
    if (index > 88) index = 88;
    size_t out = 0;
    size_t i = 4;
    while (i < adpcm_len && out < pcm_cap_samples) {
        uint8_t byte = adpcm[i++];
        for (int n = 0; n < 2 && out < pcm_cap_samples; ++n) {
            uint8_t code = (n == 0) ? (byte & 0x0F) : (byte >> 4);
            int step = IMA_STEP[index];
            int diffq = step >> 3;
            if (code & 4) diffq += step;
            if (code & 2) diffq += step >> 1;
            if (code & 1) diffq += step >> 2;
            if (code & 8) pred -= diffq;
            else pred += diffq;
            if (pred > 32767) pred = 32767;
            else if (pred < -32768) pred = -32768;
            index += IMA_INDEX[code & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;
            pcm_out[out++] = (int16_t)pred;
        }
    }
    return out;
}
