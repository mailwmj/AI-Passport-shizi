#pragma once

#include "esp_err.h"
#include "bsp_button.h"
#include "hanzi_deck.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KIDS_SCREEN_HOME = 0,
    KIDS_SCREEN_HELP,
    KIDS_SCREEN_LEARN,
    KIDS_SCREEN_QUIZ_PICK,
    KIDS_SCREEN_QUIZ,
    KIDS_SCREEN_RESULT,
} kids_screen_t;

esp_err_t kids_ui_start(hanzi_progress_t *progress);
void kids_ui_on_button(bsp_btn_t btn, bsp_btn_ev_t ev);
kids_screen_t kids_ui_screen(void);

#ifdef __cplusplus
}
#endif
