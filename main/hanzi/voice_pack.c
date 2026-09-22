#include "voice_pack.h"
#include "esp_log.h"
#include <string.h>

/* Embedded by CMake EMBED_FILES as hanzi_voice_pack.bin */
extern const uint8_t hanzi_voice_pack_bin_start[] asm("_binary_hanzi_voice_pack_bin_start");
extern const uint8_t hanzi_voice_pack_bin_end[] asm("_binary_hanzi_voice_pack_bin_end");

static const char *TAG = "voice_pack";

typedef struct {
    uint16_t index_global;
    uint16_t reserved;
    uint32_t offset;
    uint16_t adpcm_len;
    uint16_t samples;
} voice_entry_t;

static const uint8_t *s_base;
static size_t s_size;
static uint16_t s_count;
static const voice_entry_t *s_table;
static bool s_ok;

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool voice_pack_init(void)
{
    s_base = hanzi_voice_pack_bin_start;
    s_size = (size_t)(hanzi_voice_pack_bin_end - hanzi_voice_pack_bin_start);
    s_ok = false;
    s_count = 0;
    s_table = NULL;
    if (s_size < 12) {
        ESP_LOGW(TAG, "voice pack missing/empty (%u bytes)", (unsigned)s_size);
        return false;
    }
    if (memcmp(s_base, "HZVP", 4) != 0) {
        ESP_LOGE(TAG, "bad magic");
        return false;
    }
    uint16_t ver = rd_u16(s_base + 4);
    uint16_t rate = rd_u16(s_base + 6);
    s_count = rd_u16(s_base + 8);
    if (ver != 1 || rate != 16000) {
        ESP_LOGE(TAG, "unsupported ver=%u rate=%u", ver, rate);
        return false;
    }
    size_t need = 12 + (size_t)s_count * 12;
    if (s_size < need) {
        ESP_LOGE(TAG, "truncated pack");
        return false;
    }
    s_table = (const voice_entry_t *)(s_base + 12);
    /* Validate entries are within file (manual field read — packed little-endian) */
    for (uint16_t i = 0; i < s_count; ++i) {
        const uint8_t *e = s_base + 12 + (size_t)i * 12;
        uint32_t off = rd_u32(e + 4);
        uint16_t len = rd_u16(e + 8);
        if (off + len > s_size) {
            ESP_LOGE(TAG, "entry %u OOB", i);
            return false;
        }
    }
    s_ok = true;
    ESP_LOGI(TAG, "voice pack ready: %u clips, %u bytes", s_count, (unsigned)s_size);
    return true;
}

uint16_t voice_pack_count(void)
{
    return s_ok ? s_count : 0;
}

bool voice_pack_find(uint16_t index_global_1based, voice_clip_t *out)
{
    if (!s_ok || !out || !s_base) return false;
    /* Linear search is fine for 552; table is sorted by generation order. */
    for (uint16_t i = 0; i < s_count; ++i) {
        const uint8_t *e = s_base + 12 + (size_t)i * 12;
        uint16_t idx = rd_u16(e);
        if (idx != index_global_1based) continue;
        uint32_t off = rd_u32(e + 4);
        uint16_t len = rd_u16(e + 8);
        uint16_t samples = rd_u16(e + 10);
        out->index_global = idx;
        out->sample_count = samples;
        out->adpcm = s_base + off;
        out->adpcm_len = len;
        return true;
    }
    return false;
}
