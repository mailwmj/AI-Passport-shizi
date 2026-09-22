#include "audio_worker.h"
#include "voice_pack.h"
#include "voice_adpcm.h"
#include "bsp_audio.h"
#include "settings_nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

static const char *TAG = "audio_worker";

#define SAMPLE_RATE   16000
#define CHUNK_SAMPLES 256
#define QUEUE_LEN     4
#define MAX_DECODE_SAMPLES (SAMPLE_RATE * 6 / 10) /* 0.6 s cap */

typedef enum {
    AW_CMD_PLAY_HANZI = 1,
    AW_CMD_TONE,
    AW_CMD_STOP,
} aw_cmd_type_t;

typedef struct {
    aw_cmd_type_t type;
    uint16_t index_global;
} aw_cmd_t;

static QueueHandle_t s_q;
static TaskHandle_t s_task;
static volatile bool s_cancel;
static uint8_t s_volume = SETTINGS_VOLUME_DEFAULT;

static uint8_t clamp_vol(uint8_t v)
{
    if (v < SETTINGS_VOLUME_MIN) return SETTINGS_VOLUME_MIN;
    if (v > SETTINGS_VOLUME_MAX) return SETTINGS_VOLUME_MAX;
    return v;
}

/* UI 100% = codec 100 + digital boost for outdoor kids use. Fixed 1.75x. */
#define PCM_DIGITAL_GAIN_NUM 7
#define PCM_DIGITAL_GAIN_DEN 4

static int16_t apply_pcm_gain(int16_t sample)
{
    int32_t s = ((int32_t)sample * PCM_DIGITAL_GAIN_NUM) / PCM_DIGITAL_GAIN_DEN;
    if (s > 32767) return 32767;
    if (s < -32768) return (int16_t)-32768;
    return (int16_t)s;
}

static void play_pcm(const int16_t *pcm, size_t samples)
{
    int16_t chunk[CHUNK_SAMPLES];
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "set_format failed");
        return;
    }
    bsp_audio_set_volume(s_volume);
    size_t off = 0;
    while (off < samples && !s_cancel) {
        size_t n = samples - off;
        if (n > CHUNK_SAMPLES) n = CHUNK_SAMPLES;
        for (size_t i = 0; i < n; ++i) {
            chunk[i] = apply_pcm_gain(pcm[off + i]);
        }
        if (bsp_audio_write(chunk, n * sizeof(int16_t)) != ESP_OK) break;
        off += n;
    }
}

static void play_tone(void)
{
    int16_t buf[CHUNK_SAMPLES];
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) return;
    bsp_audio_set_volume(s_volume);
    const int period = SAMPLE_RATE / 880;
    int total = SAMPLE_RATE / 5;
    int phase = 0;
    while (total > 0 && !s_cancel) {
        int n = total < CHUNK_SAMPLES ? total : CHUNK_SAMPLES;
        for (int i = 0; i < n; ++i) {
            int16_t raw = (phase < period / 2) ? 5000 : -5000;
            buf[i] = apply_pcm_gain(raw);
            if (++phase >= period) phase = 0;
        }
        if (bsp_audio_write(buf, (size_t)n * sizeof(int16_t)) != ESP_OK) break;
        total -= n;
    }
}

static void play_hanzi(uint16_t index_global)
{
    voice_clip_t clip;
    if (!voice_pack_find(index_global, &clip)) {
        ESP_LOGW(TAG, "no voice for #%u", (unsigned)index_global);
        play_tone();
        return;
    }
    size_t cap = clip.sample_count ? clip.sample_count : (clip.adpcm_len * 2);
    if (cap > MAX_DECODE_SAMPLES) cap = MAX_DECODE_SAMPLES;
    if (cap < 32) cap = 32;
    int16_t *pcm = (int16_t *)malloc(cap * sizeof(int16_t));
    if (!pcm) {
        ESP_LOGE(TAG, "PCM alloc fail");
        play_tone();
        return;
    }
    size_t ns = voice_adpcm_decode(clip.adpcm, clip.adpcm_len, pcm, cap);
    if (ns == 0) {
        free(pcm);
        play_tone();
        return;
    }
    play_pcm(pcm, ns);
    free(pcm);
}

static void worker(void *arg)
{
    (void)arg;
    aw_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_q, &cmd, portMAX_DELAY) != pdTRUE) continue;
        if (cmd.type == AW_CMD_STOP) break;
        s_cancel = false;
        if (cmd.type == AW_CMD_PLAY_HANZI) play_hanzi(cmd.index_global);
        else if (cmd.type == AW_CMD_TONE) play_tone();
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_worker_start(void)
{
    if (s_task) return ESP_OK;
    s_q = xQueueCreate(QUEUE_LEN, sizeof(aw_cmd_t));
    if (!s_q) return ESP_ERR_NO_MEM;
    if (xTaskCreate(worker, "hanzi_audio", 4096, NULL, 5, &s_task) != pdPASS) {
        vQueueDelete(s_q);
        s_q = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void audio_worker_stop(void)
{
    if (!s_q) return;
    s_cancel = true;
    aw_cmd_t cmd = { .type = AW_CMD_STOP, .index_global = 0 };
    (void)xQueueSend(s_q, &cmd, pdMS_TO_TICKS(100));
    for (int i = 0; i < 50 && s_task; ++i) vTaskDelay(pdMS_TO_TICKS(20));
    if (s_q) {
        vQueueDelete(s_q);
        s_q = NULL;
    }
}

void audio_worker_play_hanzi(uint16_t index_global_1based)
{
    if (!s_q) return;
    s_cancel = true;
    aw_cmd_t cmd = { .type = AW_CMD_PLAY_HANZI, .index_global = index_global_1based };
    (void)xQueueSend(s_q, &cmd, 0);
}

void audio_worker_play_tone_fallback(void)
{
    if (!s_q) return;
    aw_cmd_t cmd = { .type = AW_CMD_TONE, .index_global = 0 };
    (void)xQueueSend(s_q, &cmd, 0);
}

void audio_worker_set_volume(uint8_t percent)
{
    s_volume = clamp_vol(percent);
    bsp_audio_set_volume(s_volume);
}

uint8_t audio_worker_get_volume(void)
{
    return s_volume;
}
