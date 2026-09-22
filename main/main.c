/* Kids pink 小儿识字 — replaces the demo menu shell. */
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_pins.h"
#include "hanzi_deck.h"
#include "progress_nvs.h"
#include "settings_nvs.h"
#include "voice_pack.h"
#include "audio_worker.h"
#include "kids_ui.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "kids_main";

#define INPUT_QUEUE_DEPTH 8

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;
static hanzi_progress_t s_progress;

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            kids_ui_on_button(input.btn, input.event);
        }
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = ev };
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "小儿识字 starting");

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool audio_ok = (bsp_audio_init() == ESP_OK);
    if (!audio_ok) {
        ESP_LOGW(TAG, "audio init failed — voice playback disabled");
    }

    (void)voice_pack_init();
    if (audio_ok && audio_worker_start() != ESP_OK) {
        ESP_LOGW(TAG, "audio worker start failed");
    }

    (void)progress_nvs_load(&s_progress);

    uint8_t volume = SETTINGS_VOLUME_DEFAULT;
    (void)settings_nvs_load_volume(&volume);
    audio_worker_set_volume(volume);

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue ||
        xTaskCreate(input_task, "kids_input", 4096, NULL, 5, &s_input_task) != pdPASS) {
        ESP_LOGE(TAG, "input task create failed");
        return;
    }

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }

    if (kids_ui_start(&s_progress) != ESP_OK) {
        ESP_LOGE(TAG, "UI start failed");
        return;
    }
    s_input_ready = true;
    ESP_LOGI(TAG, "ready: voices=%u book=%u stars=%u",
             (unsigned)voice_pack_count(),
             (unsigned)s_progress.book,
             (unsigned)s_progress.mastered_count);
}
