#include "progress_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "progress_nvs";
static const char *NS = "kids_hanzi";

esp_err_t progress_nvs_load(hanzi_progress_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    hanzi_progress_init(out);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    uint8_t book = 1;
    uint16_t legacy_idx = 0;
    size_t stars_len = sizeof(out->stars);
    if (nvs_get_u8(h, "book", &book) == ESP_OK) {
        /* 0 = 我的收藏; 1..SIWU_BOOK_COUNT = books; else clamp */
        if (book == HANZI_DECK_FAVORITES || (book >= 1 && book <= SIWU_BOOK_COUNT)) {
            hanzi_deck_select_book(out, book);
        } else {
            hanzi_deck_select_book(out, 1);
        }
    } else if (nvs_get_u16(h, "idx", &legacy_idx) == ESP_OK) {
        /* Migrate old global index → book */
        hanzi_deck_select_book(out, hanzi_deck_book_of(legacy_idx));
    }
    (void)nvs_get_blob(h, "stars", out->stars, &stars_len);
    nvs_close(h);
    hanzi_progress_recount(out);
    ESP_LOGI(TAG, "loaded book=%u favorites=%u", out->book, out->mastered_count);
    return ESP_OK;
}

esp_err_t progress_nvs_save(const hanzi_progress_t *in)
{
    if (!in) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, "book", in->book);
    if (err == ESP_OK) err = nvs_set_blob(h, "stars", in->stars, sizeof(in->stars));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
