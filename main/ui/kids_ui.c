#include "kids_ui.h"
#include "audio_worker.h"
#include "settings_nvs.h"
#include "progress_nvs.h"
#include "siwu_hanzi_table.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(kids_font_56); /* Learn: 88px bpp2 (ABI name kept) */
LV_FONT_DECLARE(kids_font_48); /* Quiz option glyphs */
LV_FONT_DECLARE(kids_font_32); /* Home center titles */
LV_FONT_DECLARE(kids_font_20);
LV_IMAGE_DECLARE(thumb_up);

/* Soft pink kids theme */
#define COL_BG       0xFFE4F0
#define COL_BG_DEEP  0xFFD6E8
#define COL_CARD     0xFFFFFF
#define COL_EDGE     0xFF78B4
#define COL_SHADOW   0xD870A0
#define COL_PILL     0xFFF0F7
#define COL_PILL_BD  0xFFB7D5
#define COL_INK      0x5A1A3A
#define COL_ACCENT   0xFF4D8D
#define COL_MUTE     0xC47A9A
#define COL_STAR     0xFFC107
#define COL_DOT      0xFF8FB8
#define COL_WHITE    0xFFFFFF
#define COL_OK       0xFF6FA8
#define COL_TIP_BG   0x5A1A3A
#define COL_DOT_OFF  0xE68AB4
#define COL_CTA_SH   0xE03A72

/* Home carousel: 0 help, 1 quiz, 2 favorites, 3..9 books 1..7 */
#define HOME_SLOT_HELP   0
#define HOME_SLOT_QUIZ   1
#define HOME_SLOT_FAV    2
#define HOME_SLOT_BOOK1  3
#define HOME_SLOT_COUNT  10

#define QUIZ_PICK_COUNT  8 /* favorites + books 1..7 */
#define QUIZ_PICK_VISIBLE 3
#define RESULT_GRID_COLS 4
#define RESULT_GRID_ROWS 2
#define RESULT_PAGE_SIZE (RESULT_GRID_COLS * RESULT_GRID_ROWS)

static const char *TAG = "kids_ui";

static hanzi_progress_t *s_prog;
static kids_screen_t s_screen = KIDS_SCREEN_HOME;
static uint8_t s_home_slot = HOME_SLOT_BOOK1;

static lv_obj_t *s_home;
static lv_obj_t *s_help;
static lv_obj_t *s_learn;
static lv_obj_t *s_quiz_pick;
static lv_obj_t *s_quiz;
static lv_obj_t *s_result;

/* Home carousel */
static lv_obj_t *s_card_center;
static lv_obj_t *s_card_left;
static lv_obj_t *s_card_right;
static lv_obj_t *s_c_block; /* title+stats grouped, vertically centered */
static lv_obj_t *s_c_title;
static lv_obj_t *s_c_stats;
static lv_obj_t *s_c_help_lines[2];
static lv_obj_t *s_l_title;
static lv_obj_t *s_l_stats;
static lv_obj_t *s_r_title;
static lv_obj_t *s_r_stats;
static lv_obj_t *s_home_cta_lab;
static lv_obj_t *s_home_dots[HOME_SLOT_COUNT];

/* Learn */
static lv_obj_t *s_idx_lab;
static lv_obj_t *s_star_lab;
static lv_obj_t *s_char_lab;

/* Quiz pick */
static lv_obj_t *s_pick_row[QUIZ_PICK_VISIBLE];
static lv_obj_t *s_pick_title[QUIZ_PICK_VISIBLE];
static lv_obj_t *s_pick_stats[QUIZ_PICK_VISIBLE];
static uint8_t s_pick_cursor; /* 0=fav, 1..7 = book */
static uint8_t s_pick_window; /* first visible index */

/* Quiz play */
static lv_obj_t *s_quiz_prog_lab;
static lv_obj_t *s_quiz_opt_lab[3];
static lv_obj_t *s_quiz_opt_card[3];
static lv_obj_t *s_quiz_opt_shadow[3];
static uint16_t s_quiz_opts[3];
static uint16_t s_quiz_correct;
static uint8_t s_quiz_cursor;
static uint16_t s_quiz_qi;
static uint16_t s_quiz_answered;
static uint16_t s_quiz_ok_count;
static uint16_t s_quiz_wrong[SIWU_MAX_BOOK_CHARS];
static uint16_t s_quiz_wrong_count;
static uint8_t s_quiz_scope_book; /* favorites or 1..7 */
static bool s_quiz_busy; /* awaiting feedback advance */
static lv_timer_t *s_quiz_advance_timer;
static bool s_quiz_scored_this; /* first-attempt scoring latch for current question */
static uint8_t s_quiz_retry_n;

/* Result */
static lv_obj_t *s_res_title;
static lv_obj_t *s_res_sub;
static lv_obj_t *s_res_stats;
static lv_obj_t *s_res_thumb_box;
static lv_obj_t *s_res_wrong_card;
static lv_obj_t *s_res_wrong_lab;
static lv_obj_t *s_res_grid_lab[RESULT_PAGE_SIZE];
static lv_obj_t *s_res_grid_cell[RESULT_PAGE_SIZE];
static lv_obj_t *s_res_page_hint;
static lv_obj_t *s_res_cta_lab;
static uint16_t s_res_wrong_page;

static lv_obj_t *s_tip;
static lv_obj_t *s_tip_lab;
static lv_timer_t *s_tip_timer;

static uint32_t s_enter_nonce;
static bool s_learn_return_tip_shown;

static void style_label(lv_obj_t *lab, const lv_font_t *font, uint32_t color)
{
    lv_obj_set_style_text_font(lab, font, 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(color), 0);
}

static lv_obj_t *make_screen(uint32_t bg)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static void add_dot(lv_obj_t *parent, int x, int y, int r, uint32_t color)
{
    lv_obj_t *d = lv_obj_create(parent);
    lv_obj_set_pos(d, x, y);
    lv_obj_set_size(d, r, r);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_60, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *make_soft_card(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *sh = lv_obj_create(parent);
    lv_obj_set_pos(sh, x, y + 6);
    lv_obj_set_size(sh, w, h);
    lv_obj_set_style_bg_color(sh, lv_color_hex(COL_SHADOW), 0);
    lv_obj_set_style_bg_opa(sh, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sh, 22, 0);
    lv_obj_set_style_border_width(sh, 0, 0);
    lv_obj_set_style_pad_all(sh, 0, 0);
    lv_obj_clear_flag(sh, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(sh, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 22, 0);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_EDGE), 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    /* Shadow is a sibling; keep pointer so hide/show can cover both. */
    lv_obj_set_user_data(card, sh);
    return card;
}

static void soft_card_set_hidden(lv_obj_t *card, bool hidden)
{
    if (!card) return;
    lv_obj_t *sh = lv_obj_get_user_data(card);
    if (hidden) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        if (sh) lv_obj_add_flag(sh, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(card, LV_OBJ_FLAG_HIDDEN);
        if (sh) lv_obj_clear_flag(sh, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_back_chip(lv_obj_t *parent)
{
    lv_obj_t *back = lv_obj_create(parent);
    lv_obj_set_pos(back, 10, 8);
    lv_obj_set_size(back, 28, 28);
    lv_obj_set_style_bg_color(back, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *back_lab = lv_label_create(back);
    style_label(back_lab, &kids_font_20, COL_WHITE);
    lv_label_set_text(back_lab, "‹");
    lv_obj_align(back_lab, LV_ALIGN_CENTER, 2, -1);
    return back;
}

static lv_obj_t *make_cta(lv_obj_t *parent, int x, int y, int w, int h, const char *text,
                         lv_obj_t **out_lab)
{
    lv_obj_t *sh = lv_obj_create(parent);
    lv_obj_set_pos(sh, x, y + 4);
    lv_obj_set_size(sh, w, h);
    lv_obj_set_style_bg_color(sh, lv_color_hex(COL_CTA_SH), 0);
    lv_obj_set_style_bg_opa(sh, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sh, h / 2, 0);
    lv_obj_set_style_border_width(sh, 0, 0);
    lv_obj_clear_flag(sh, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cta = lv_obj_create(parent);
    lv_obj_set_pos(cta, x, y);
    lv_obj_set_size(cta, w, h);
    lv_obj_set_style_bg_color(cta, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_bg_opa(cta, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cta, h / 2, 0);
    lv_obj_set_style_border_width(cta, 0, 0);
    lv_obj_set_style_pad_all(cta, 0, 0);
    lv_obj_clear_flag(cta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lab = lv_label_create(cta);
    style_label(lab, &kids_font_20, COL_WHITE);
    lv_label_set_text(lab, text);
    lv_obj_center(lab);
    if (out_lab) *out_lab = lab;
    return cta;
}

static void persist(void)
{
    if (s_prog) {
        (void)progress_nvs_save(s_prog);
    }
}

static uint16_t cur_index(void)
{
    return hanzi_deck_current(s_prog);
}

static void play_current(void)
{
    if (!s_prog) return;
    const siwu_hanzi_t *h = siwu_hanzi_get(cur_index());
    if (h) audio_worker_play_hanzi(h->index_global);
}

static void tip_hide_cb(lv_timer_t *t)
{
    (void)t;
    if (s_tip) {
        lv_obj_add_flag(s_tip, LV_OBJ_FLAG_HIDDEN);
    }
    s_tip_timer = NULL;
}

static void show_tip_ms(lv_obj_t *parent, const char *msg, uint32_t ms)
{
    if (!parent || !msg) return;
    if (!s_tip) {
        s_tip = lv_obj_create(parent);
        lv_obj_set_size(s_tip, 200, 36);
        lv_obj_set_style_bg_color(s_tip, lv_color_hex(COL_TIP_BG), 0);
        lv_obj_set_style_bg_opa(s_tip, LV_OPA_80, 0);
        lv_obj_set_style_radius(s_tip, 14, 0);
        lv_obj_set_style_border_width(s_tip, 0, 0);
        lv_obj_set_style_pad_hor(s_tip, 8, 0);
        lv_obj_set_style_pad_ver(s_tip, 4, 0);
        lv_obj_clear_flag(s_tip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(s_tip, LV_OBJ_FLAG_CLICKABLE);
        s_tip_lab = lv_label_create(s_tip);
        style_label(s_tip_lab, &kids_font_20, COL_WHITE);
        lv_obj_center(s_tip_lab);
    } else {
        lv_obj_set_parent(s_tip, parent);
    }
    lv_label_set_text(s_tip_lab, msg);
    lv_obj_center(s_tip_lab);
    lv_obj_align(s_tip, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_tip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_tip);
    if (s_tip_timer) {
        lv_timer_delete(s_tip_timer);
        s_tip_timer = NULL;
    }
    s_tip_timer = lv_timer_create(tip_hide_cb, ms ? ms : 900, NULL);
    lv_timer_set_repeat_count(s_tip_timer, 1);
}

static void show_tip(lv_obj_t *parent, const char *msg)
{
    show_tip_ms(parent, msg, 900);
}

static uint8_t book_to_home_slot(uint8_t book)
{
    if (hanzi_deck_is_favorites(book)) return HOME_SLOT_FAV;
    if (book >= 1 && book <= SIWU_BOOK_COUNT) {
        return (uint8_t)(HOME_SLOT_BOOK1 + book - 1);
    }
    return HOME_SLOT_BOOK1;
}

static bool home_slot_is_bookish(uint8_t slot)
{
    return slot == HOME_SLOT_FAV || slot >= HOME_SLOT_BOOK1;
}

static uint8_t home_slot_to_book(uint8_t slot)
{
    if (slot == HOME_SLOT_FAV) return HANZI_DECK_FAVORITES;
    if (slot >= HOME_SLOT_BOOK1 && slot < HOME_SLOT_COUNT) {
        return (uint8_t)(slot - HOME_SLOT_BOOK1 + 1);
    }
    return 1;
}

static void sync_book_from_home_slot(void)
{
    if (!s_prog) return;
    if (home_slot_is_bookish(s_home_slot)) {
        hanzi_deck_select_book(s_prog, home_slot_to_book(s_home_slot));
    }
}

static void fill_book_labels(uint8_t book, lv_obj_t *title, lv_obj_t *stats)
{
    char buf[48];
    /* Center-card only: mid title; peeks never call this. */
    style_label(title, &kids_font_32, COL_INK);
    style_label(stats, &kids_font_20, COL_INK);
    if (hanzi_deck_is_favorites(book)) {
        uint16_t n = hanzi_deck_favorite_count(s_prog);
        lv_label_set_text(title, "我的收藏");
        snprintf(buf, sizeof(buf), "收藏 %u", (unsigned)n);
        lv_label_set_text(stats, buf);
        return;
    }
    if (book < 1 || book > SIWU_BOOK_COUNT) book = 1;
    uint16_t learned = hanzi_deck_learned_in_book(s_prog, book);
    uint16_t total = g_siwu_book_counts[book - 1];
    snprintf(buf, sizeof(buf), "第%u册", (unsigned)book);
    lv_label_set_text(title, buf);
    snprintf(buf, sizeof(buf), "★ %u / %u", (unsigned)learned, (unsigned)total);
    lv_label_set_text(stats, buf);
}

static void fill_home_slot_labels(uint8_t slot, lv_obj_t *title, lv_obj_t *stats, bool peek)
{
    char buf[32];
    if (slot == HOME_SLOT_HELP) {
        style_label(title, &kids_font_20, peek ? COL_MUTE : COL_INK);
        style_label(stats, &kids_font_20, peek ? COL_MUTE : COL_INK);
        lv_label_set_text(title, "怎么用");
        lv_label_set_text(stats, peek ? "说明" : "");
        return;
    }
    if (slot == HOME_SLOT_QUIZ) {
        /* Peeks stay 20px — 48/32 on peeks previously OOM'd the home screen. */
        style_label(title, peek ? &kids_font_20 : &kids_font_32, peek ? COL_MUTE : COL_INK);
        style_label(stats, &kids_font_20, peek ? COL_MUTE : COL_INK);
        lv_label_set_text(title, peek ? "测验" : "开始测验");
        lv_label_set_text(stats, peek ? "听选" : "先选册或收藏");
        return;
    }
    if (slot == HOME_SLOT_FAV) {
        if (peek) {
            style_label(title, &kids_font_20, COL_MUTE);
            style_label(stats, &kids_font_20, COL_MUTE);
            lv_label_set_text(title, "收藏");
            snprintf(buf, sizeof(buf), "%u", (unsigned)hanzi_deck_favorite_count(s_prog));
            lv_label_set_text(stats, buf);
            return;
        }
        fill_book_labels(HANZI_DECK_FAVORITES, title, stats);
        return;
    }
    uint8_t book = home_slot_to_book(slot);
    if (peek) {
        style_label(title, &kids_font_20, COL_MUTE);
        style_label(stats, &kids_font_20, COL_MUTE);
        snprintf(buf, sizeof(buf), "第%u册", (unsigned)book);
        lv_label_set_text(title, buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)g_siwu_book_counts[book - 1]);
        lv_label_set_text(stats, buf);
        return;
    }
    fill_book_labels(book, title, stats);
}

static void refresh_home(void)
{
    if (!s_card_center || !s_prog) return;
    if (s_home_slot >= HOME_SLOT_COUNT) s_home_slot = HOME_SLOT_BOOK1;

    uint8_t prev = (uint8_t)((s_home_slot + HOME_SLOT_COUNT - 1) % HOME_SLOT_COUNT);
    uint8_t next = (uint8_t)((s_home_slot + 1) % HOME_SLOT_COUNT);

    bool help_center = (s_home_slot == HOME_SLOT_HELP);
    for (int i = 0; i < 2; ++i) {
        if (s_c_help_lines[i]) {
            if (help_center) lv_obj_clear_flag(s_c_help_lines[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(s_c_help_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_c_stats) {
        if (help_center) lv_obj_add_flag(s_c_stats, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(s_c_stats, LV_OBJ_FLAG_HIDDEN);
    }

    fill_home_slot_labels(s_home_slot, s_c_title, s_c_stats, false);
    fill_home_slot_labels(prev, s_l_title, s_l_stats, true);
    fill_home_slot_labels(next, s_r_title, s_r_stats, true);

    /* Help: small top title + 2 lines. Book/fav/quiz: title+stats block centered. */
    if (s_c_block) {
        if (help_center) lv_obj_align(s_c_block, LV_ALIGN_TOP_MID, 0, 0);
        else lv_obj_align(s_c_block, LV_ALIGN_CENTER, 0, 0);
    }

    const char *cta = "开始学习";
    if (s_home_slot == HOME_SLOT_HELP) cta = "查看说明";
    else if (s_home_slot == HOME_SLOT_QUIZ) cta = "去测验";
    else if (s_home_slot == HOME_SLOT_FAV) cta = "打开收藏";
    lv_label_set_text(s_home_cta_lab, cta);

    const int dot_y = 268;
    const int spacing = 12;
    const int first_cx = (240 - (HOME_SLOT_COUNT - 1) * spacing) / 2;
    for (int i = 0; i < HOME_SLOT_COUNT; ++i) {
        bool on = (i == (int)s_home_slot);
        int sz = on ? 10 : 7;
        int cx = first_cx + i * spacing;
        lv_obj_set_size(s_home_dots[i], sz, sz);
        lv_obj_set_pos(s_home_dots[i], cx - sz / 2, dot_y - (sz - 7) / 2);
        lv_obj_set_style_bg_color(s_home_dots[i],
                                  lv_color_hex(on ? COL_ACCENT : COL_DOT_OFF), 0);
        lv_obj_set_style_bg_opa(s_home_dots[i], LV_OPA_COVER, 0);
    }
}

static void refresh_learn(void)
{
    if (!s_char_lab || !s_prog) return;
    uint16_t idx = cur_index();
    const siwu_hanzi_t *h = siwu_hanzi_get(idx);
    if (!h) return;
    lv_label_set_text(s_char_lab, h->utf8);

    char idxbuf[32];
    if (hanzi_deck_is_favorites(s_prog->book)) {
        snprintf(idxbuf, sizeof(idxbuf), "★ %u/%u",
                 (unsigned)(s_prog->session_pos + 1u),
                 (unsigned)s_prog->session_len);
    } else {
        snprintf(idxbuf, sizeof(idxbuf), "%u/%u",
                 (unsigned)(s_prog->session_pos + 1u),
                 (unsigned)s_prog->session_len);
    }
    lv_label_set_text(s_idx_lab, idxbuf);

    bool star = hanzi_progress_is_starred(s_prog, idx);
    lv_label_set_text(s_star_lab, star ? "★" : "☆");
    lv_obj_set_style_text_color(s_star_lab, lv_color_hex(star ? COL_STAR : COL_MUTE), 0);
}

static void pick_scope_labels(uint8_t idx, char *title, size_t tlen, char *stats, size_t slen)
{
    (void)stats;
    (void)slen;
    if (idx == 0) {
        snprintf(title, tlen, "我的收藏 · %u字",
                 (unsigned)hanzi_deck_favorite_count(s_prog));
    } else {
        uint8_t book = idx; /* 1..7 */
        snprintf(title, tlen, "第 %u 册 · %u字",
                 (unsigned)book,
                 (unsigned)g_siwu_book_counts[book - 1]);
    }
}

static void refresh_quiz_pick(void)
{
    if (!s_pick_row[0]) return;
    if (s_pick_cursor >= QUIZ_PICK_COUNT) s_pick_cursor = 0;
    if (s_pick_cursor < s_pick_window) {
        s_pick_window = s_pick_cursor;
    } else if (s_pick_cursor >= s_pick_window + QUIZ_PICK_VISIBLE) {
        s_pick_window = (uint8_t)(s_pick_cursor - QUIZ_PICK_VISIBLE + 1);
    }
    char tbuf[40], sbuf[8];
    for (int i = 0; i < QUIZ_PICK_VISIBLE; ++i) {
        uint8_t idx = (uint8_t)(s_pick_window + i);
        pick_scope_labels(idx, tbuf, sizeof(tbuf), sbuf, sizeof(sbuf));
        lv_label_set_text(s_pick_title[i], tbuf);
        lv_label_set_text(s_pick_stats[i], "");
        bool sel = (idx == s_pick_cursor);
        lv_obj_set_style_border_color(s_pick_row[i],
                                      lv_color_hex(sel ? COL_ACCENT : COL_EDGE), 0);
        lv_obj_set_style_border_width(s_pick_row[i], sel ? 3 : 2, 0);
        lv_obj_set_style_text_color(s_pick_title[i],
                                    lv_color_hex(sel ? COL_ACCENT : COL_INK), 0);
    }
}

static void refresh_quiz_opts(void)
{
    for (int i = 0; i < 3; ++i) {
        const siwu_hanzi_t *h = siwu_hanzi_get(s_quiz_opts[i]);
        lv_label_set_text(s_quiz_opt_lab[i], h ? h->utf8 : "?");
        bool sel = (i == (int)s_quiz_cursor);
        /* Selected = solid pink fill + white glyph (much clearer than thin border). */
        lv_obj_set_style_bg_color(s_quiz_opt_card[i],
                                  lv_color_hex(sel ? COL_ACCENT : COL_CARD), 0);
        lv_obj_set_style_border_color(s_quiz_opt_card[i],
                                      lv_color_hex(sel ? COL_ACCENT : COL_EDGE), 0);
        lv_obj_set_style_border_width(s_quiz_opt_card[i], sel ? 4 : 2, 0);
        lv_obj_set_style_text_color(s_quiz_opt_lab[i],
                                    lv_color_hex(sel ? COL_WHITE : COL_INK), 0);
    }
}

static void refresh_quiz_progress(void)
{
    if (!s_quiz_prog_lab || !s_prog) return;
    char buf[24];
    uint16_t n = (uint16_t)(s_quiz_qi + 1u);
    uint16_t N = s_prog->session_len ? s_prog->session_len : 1;
    if (n > N) n = N;
    snprintf(buf, sizeof(buf), "%u / %u", (unsigned)n, (unsigned)N);
    lv_label_set_text(s_quiz_prog_lab, buf);
}

static void refresh_result(void);

static void show_screen(kids_screen_t which)
{
    s_screen = which;
    if (s_tip) {
        lv_obj_add_flag(s_tip, LV_OBJ_FLAG_HIDDEN);
    }
    switch (which) {
    case KIDS_SCREEN_HOME:
        refresh_home();
        lv_screen_load(s_home);
        break;
    case KIDS_SCREEN_HELP:
        lv_screen_load(s_help);
        break;
    case KIDS_SCREEN_LEARN:
        refresh_learn();
        lv_screen_load(s_learn);
        break;
    case KIDS_SCREEN_QUIZ_PICK:
        refresh_quiz_pick();
        lv_screen_load(s_quiz_pick);
        break;
    case KIDS_SCREEN_QUIZ:
        lv_screen_load(s_quiz);
        break;
    case KIDS_SCREEN_RESULT:
        refresh_result();
        lv_screen_load(s_result);
        break;
    default:
        lv_screen_load(s_home);
        break;
    }
}

static void cancel_quiz_advance(void)
{
    if (s_quiz_advance_timer) {
        lv_timer_delete(s_quiz_advance_timer);
        s_quiz_advance_timer = NULL;
    }
    s_quiz_busy = false;
}

static void load_quiz_question(void)
{
    if (!s_prog || s_prog->session_len == 0) return;
    if (s_quiz_qi >= s_prog->session_len) return;
    s_prog->session_pos = s_quiz_qi;
    s_quiz_correct = s_prog->session_order[s_quiz_qi];
    s_quiz_scored_this = false;
    s_quiz_retry_n = 0;
    hanzi_quiz_options(s_quiz_correct, s_quiz_opts,
                       (uint32_t)s_quiz_correct * 97u + (uint32_t)s_quiz_qi * 13u + 1u);
    s_quiz_cursor = 0;
    refresh_quiz_progress();
    refresh_quiz_opts();
    const siwu_hanzi_t *h = siwu_hanzi_get(s_quiz_correct);
    if (h) audio_worker_play_hanzi(h->index_global);
}

static void show_quiz_result(void)
{
    cancel_quiz_advance();
    s_res_wrong_page = 0;
    show_screen(KIDS_SCREEN_RESULT);
}

static void quiz_advance_next(void)
{
    cancel_quiz_advance();
    s_quiz_qi++;
    if (s_quiz_qi >= s_prog->session_len) {
        show_quiz_result();
        return;
    }
    load_quiz_question();
}

static void quiz_advance_cb(lv_timer_t *t)
{
    (void)t;
    s_quiz_advance_timer = NULL;
    s_quiz_busy = false;
    quiz_advance_next();
}

static void schedule_quiz_advance(uint32_t ms)
{
    cancel_quiz_advance();
    s_quiz_busy = true;
    s_quiz_advance_timer = lv_timer_create(quiz_advance_cb, ms, NULL);
    lv_timer_set_repeat_count(s_quiz_advance_timer, 1);
}

static void quiz_retry_cb(lv_timer_t *t)
{
    (void)t;
    s_quiz_advance_timer = NULL;
    s_quiz_busy = false;
}

static void schedule_quiz_retry_pause(uint32_t ms)
{
    cancel_quiz_advance();
    s_quiz_busy = true;
    s_quiz_advance_timer = lv_timer_create(quiz_retry_cb, ms, NULL);
    lv_timer_set_repeat_count(s_quiz_advance_timer, 1);
}

static void record_wrong(uint16_t idx)
{
    for (uint16_t i = 0; i < s_quiz_wrong_count; ++i) {
        if (s_quiz_wrong[i] == idx) return;
    }
    if (s_quiz_wrong_count < SIWU_MAX_BOOK_CHARS) {
        s_quiz_wrong[s_quiz_wrong_count++] = idx;
    }
}

static void submit_quiz_answer(void)
{
    if (s_quiz_busy) return;
    bool ok = (s_quiz_opts[s_quiz_cursor] == s_quiz_correct);
    if (!s_quiz_scored_this) {
        s_quiz_scored_this = true;
        s_quiz_answered++;
        if (ok) {
            s_quiz_ok_count++;
        } else {
            record_wrong(s_quiz_correct);
        }
    }
    if (ok) {
        show_tip_ms(s_quiz, "答对啦", 500);
        schedule_quiz_advance(550);
    } else {
        s_quiz_retry_n++;
        hanzi_quiz_options(s_quiz_correct, s_quiz_opts,
                           (uint32_t)s_quiz_correct * 97u + (uint32_t)s_quiz_qi * 13u
                           + (uint32_t)s_quiz_retry_n * 31u + 1u);
        s_quiz_cursor = 0;
        refresh_quiz_opts();
        const siwu_hanzi_t *h = siwu_hanzi_get(s_quiz_correct);
        if (h) audio_worker_play_hanzi(h->index_global);
        show_tip_ms(s_quiz, "再听听~", 900);
        schedule_quiz_retry_pause(900);
    }
}

static void start_quiz_play(uint8_t scope_book)
{
    if (!s_prog) return;
    s_enter_nonce++;
    uint32_t seed = (s_enter_nonce * 2654435761u) ^ ((uint32_t)scope_book << 16) ^ 0xA17Au;

    if (hanzi_deck_is_favorites(scope_book)) {
        if (!hanzi_deck_enter_favorites(s_prog, seed)) {
            show_tip(s_quiz_pick, "还没有收藏");
            return;
        }
    } else {
        hanzi_deck_enter_book(s_prog, scope_book, seed);
    }

    s_quiz_scope_book = scope_book;
    s_quiz_qi = 0;
    s_quiz_answered = 0;
    s_quiz_ok_count = 0;
    s_quiz_wrong_count = 0;
    cancel_quiz_advance();
    show_screen(KIDS_SCREEN_QUIZ);
    load_quiz_question();
}

static void enter_quiz_pick(void)
{
    s_pick_cursor = 1; /* default 第1册 */
    s_pick_window = 0;
    if (s_prog && home_slot_is_bookish(s_home_slot)) {
        uint8_t b = home_slot_to_book(s_home_slot);
        s_pick_cursor = hanzi_deck_is_favorites(b) ? 0 : b;
    }
    show_screen(KIDS_SCREEN_QUIZ_PICK);
}

static void enter_learn(void)
{
    sync_book_from_home_slot();
    s_enter_nonce++;
    uint32_t seed = (s_enter_nonce * 2654435761u) ^ ((uint32_t)s_prog->book << 16) ^ 0xC3C3u;
    if (hanzi_deck_is_favorites(s_prog->book)) {
        if (!hanzi_deck_enter_favorites(s_prog, seed)) {
            show_tip(s_home, "还没有收藏");
            return;
        }
    } else {
        hanzi_deck_enter_book(s_prog, s_prog->book, seed);
    }
    show_screen(KIDS_SCREEN_LEARN);
    if (!s_learn_return_tip_shown) {
        s_learn_return_tip_shown = true;
        show_tip_ms(s_learn, "双击确定 · 返回", 2000);
    }
}

static void refresh_result(void)
{
    if (!s_res_title) return;
    char buf[48];
    /* All-correct celebration only when the session finished with zero wrongs. */
    bool finished = (s_prog && s_prog->session_len > 0 && s_quiz_qi >= s_prog->session_len);
    bool all_ok = finished && s_quiz_wrong_count == 0 && s_quiz_answered > 0;
    bool early = !finished;
    uint16_t wrong_n = s_quiz_wrong_count;
    uint16_t ok_n = s_quiz_ok_count;

    if (all_ok) {
        lv_label_set_text(s_res_title, "真棒！");
        lv_obj_align(s_res_title, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_clear_flag(s_res_thumb_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_res_sub, "全对啦");
        lv_obj_align(s_res_sub, LV_ALIGN_TOP_MID, 0, 148);
        if (hanzi_deck_is_favorites(s_quiz_scope_book)) {
            snprintf(buf, sizeof(buf), "我的收藏 · %u / %u",
                     (unsigned)s_quiz_ok_count, (unsigned)s_prog->session_len);
        } else {
            snprintf(buf, sizeof(buf), "第 %u 册 · %u / %u",
                     (unsigned)s_quiz_scope_book,
                     (unsigned)s_quiz_ok_count, (unsigned)s_prog->session_len);
        }
        lv_label_set_text(s_res_stats, buf);
        lv_obj_align(s_res_stats, LV_ALIGN_TOP_MID, 0, 176);
        soft_card_set_hidden(s_res_wrong_card, true);
        lv_obj_add_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);
    } else if (early) {
        /* Mid-exit: title + score only — no progress, tips, or empty pink card. */
        lv_label_set_text(s_res_title, "先到这里");
        lv_obj_align(s_res_title, LV_ALIGN_TOP_MID, 0, 72);
        snprintf(buf, sizeof(buf), "对 %u · 错 %u", (unsigned)ok_n, (unsigned)wrong_n);
        lv_label_set_text(s_res_sub, buf);
        lv_obj_align(s_res_sub, LV_ALIGN_TOP_MID, 0, 120);
        lv_label_set_text(s_res_stats, "");
        lv_obj_add_flag(s_res_thumb_box, LV_OBJ_FLAG_HIDDEN);
        soft_card_set_hidden(s_res_wrong_card, true);
        lv_obj_add_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_res_title, "测完了");
        lv_obj_align(s_res_title, LV_ALIGN_TOP_MID, 0, 16);
        snprintf(buf, sizeof(buf), "对 %u · 错 %u", (unsigned)ok_n, (unsigned)wrong_n);
        lv_label_set_text(s_res_sub, buf);
        lv_obj_align(s_res_sub, LV_ALIGN_TOP_MID, 0, 42);
        lv_label_set_text(s_res_stats, "");
        lv_obj_add_flag(s_res_thumb_box, LV_OBJ_FLAG_HIDDEN);
        if (wrong_n == 0) {
            soft_card_set_hidden(s_res_wrong_card, true);
            lv_obj_add_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);
        } else {
            soft_card_set_hidden(s_res_wrong_card, false);
            uint16_t pages = (uint16_t)((wrong_n + RESULT_PAGE_SIZE - 1) / RESULT_PAGE_SIZE);
            if (s_res_wrong_page >= pages) s_res_wrong_page = (uint16_t)(pages - 1);
            uint16_t base = (uint16_t)(s_res_wrong_page * RESULT_PAGE_SIZE);
            for (int i = 0; i < RESULT_PAGE_SIZE; ++i) {
                uint16_t wi = (uint16_t)(base + i);
                if (wi < wrong_n) {
                    const siwu_hanzi_t *h = siwu_hanzi_get(s_quiz_wrong[wi]);
                    lv_label_set_text(s_res_grid_lab[i], h ? h->utf8 : "?");
                    lv_obj_clear_flag(s_res_grid_cell[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(s_res_grid_cell[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
            if (pages > 1) {
                lv_obj_clear_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(s_res_page_hint, "上下翻看");
            } else {
                lv_obj_add_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    lv_label_set_text(s_res_cta_lab, "双击确定返回主页");
}

static lv_obj_t *make_peek_card(lv_obj_t *parent, int x, int y, int w, int h,
                                lv_obj_t **out_title, lv_obj_t **out_stats)
{
    lv_obj_t *card = make_soft_card(parent, x, y, w, h);
    lv_obj_t *title = lv_label_create(card);
    style_label(title, &kids_font_20, COL_MUTE);
    lv_label_set_text(title, "");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_t *stats = lv_label_create(card);
    style_label(stats, &kids_font_20, COL_MUTE);
    lv_label_set_text(stats, "");
    lv_obj_align(stats, LV_ALIGN_BOTTOM_MID, 0, -18);
    *out_title = title;
    *out_stats = stats;
    return card;
}

static void build_home(void)
{
    s_home = make_screen(COL_BG);
    lv_obj_t *wash = lv_obj_create(s_home);
    lv_obj_set_pos(wash, 0, 200);
    lv_obj_set_size(wash, 240, 120);
    lv_obj_set_style_bg_color(wash, lv_color_hex(COL_BG_DEEP), 0);
    lv_obj_set_style_bg_opa(wash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wash, 0, 0);
    lv_obj_set_style_radius(wash, 0, 0);
    lv_obj_clear_flag(wash, LV_OBJ_FLAG_SCROLLABLE);

    add_dot(s_home, 16, 12, 10, COL_DOT);
    add_dot(s_home, 34, 26, 7, COL_SHADOW);
    add_dot(s_home, 212, 16, 8, COL_OK);
    add_dot(s_home, 194, 34, 6, COL_EDGE);

    lv_obj_t *title = lv_label_create(s_home);
    style_label(title, &kids_font_20, COL_ACCENT);
    lv_label_set_text(title, "小儿识字");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    const int cy = 48;
    const int ch = 140;
    const int cw = 176;
    const int cx = 32;
    const int peek_w = 100;
    const int peek_h = ch - 12;
    const int peek_y = cy + 6;
    const int left_x = -52;
    const int right_x = 192;

    s_card_left = make_peek_card(s_home, left_x, peek_y, peek_w, peek_h,
                                 &s_l_title, &s_l_stats);
    s_card_right = make_peek_card(s_home, right_x, peek_y, peek_w, peek_h,
                                  &s_r_title, &s_r_stats);

    s_card_center = make_soft_card(s_home, cx, cy, cw, ch);

    /* Title + star stats as one vertically centered column (not title mid / stats bottom). */
    s_c_block = lv_obj_create(s_card_center);
    lv_obj_set_size(s_c_block, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_c_block, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_c_block, 0, 0);
    lv_obj_set_style_pad_all(s_c_block, 0, 0);
    lv_obj_set_style_pad_row(s_c_block, 6, 0);
    lv_obj_set_flex_flow(s_c_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_c_block, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_c_block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_c_block, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(s_c_block, LV_ALIGN_CENTER, 0, 0);

    s_c_title = lv_label_create(s_c_block);
    style_label(s_c_title, &kids_font_32, COL_INK);
    lv_label_set_text(s_c_title, "第1册");

    s_c_stats = lv_label_create(s_c_block);
    style_label(s_c_stats, &kids_font_20, COL_INK);
    lv_label_set_text(s_c_stats, "★ 0 / 88");

    /* Two short lines fit the center card; full help lives on build_help. Stay 20px. */
    static const char *help_lines[] = {
        "上下换卡 · 短按听选",
        "长按收藏 · 双击返回",
    };
    for (int i = 0; i < 2; ++i) {
        s_c_help_lines[i] = lv_label_create(s_card_center);
        style_label(s_c_help_lines[i], &kids_font_20, COL_MUTE);
        lv_label_set_text(s_c_help_lines[i], help_lines[i]);
        lv_obj_align(s_c_help_lines[i], LV_ALIGN_TOP_MID, 0, 42 + i * 22);
        lv_obj_add_flag(s_c_help_lines[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_move_foreground(s_card_center);

    make_cta(s_home, 32, 200, 176, 44, "开始学习", &s_home_cta_lab);

    const int dot_y = 268;
    const int spacing = 12;
    const int first_cx = (240 - (HOME_SLOT_COUNT - 1) * spacing) / 2;
    for (int i = 0; i < HOME_SLOT_COUNT; ++i) {
        s_home_dots[i] = lv_obj_create(s_home);
        lv_obj_set_size(s_home_dots[i], 7, 7);
        lv_obj_set_pos(s_home_dots[i], first_cx + i * spacing - 3, dot_y);
        lv_obj_set_style_radius(s_home_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_home_dots[i], lv_color_hex(COL_DOT_OFF), 0);
        lv_obj_set_style_bg_opa(s_home_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_home_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_home_dots[i], 0, 0);
        lv_obj_clear_flag(s_home_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(s_home_dots[i], LV_OBJ_FLAG_CLICKABLE);
    }

}

static void build_help(void)
{
    s_help = make_screen(COL_BG);
    make_back_chip(s_help);

    lv_obj_t *title = lv_label_create(s_help);
    style_label(title, &kids_font_20, COL_ACCENT);
    lv_label_set_text(title, "怎么用");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *card = make_soft_card(s_help, 16, 44, 208, 230);

    static const char *heads[] = {
        "上 / 下", "短按确定", "长按确定", "双击确定", "长按上 / 下",
    };
    static const char *details[] = {
        "换卡、换字、换选项",
        "听读音 / 选答案 / 打开",
        "收藏（仅学习页）",
        "返回上一级",
        "调音量",
    };
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *h = lv_label_create(card);
        style_label(h, &kids_font_20, COL_INK);
        lv_label_set_text(h, heads[i]);
        lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 8 + i * 42);
        lv_obj_t *d = lv_label_create(card);
        style_label(d, &kids_font_20, COL_MUTE);
        lv_label_set_text(d, details[i]);
        lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 28 + i * 42);
    }

    lv_obj_t *foot = lv_label_create(s_help);
    style_label(foot, &kids_font_20, COL_MUTE);
    lv_label_set_text(foot, "短按确定 · 我知道了");
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void build_learn(void)
{
    s_learn = make_screen(COL_BG);
    add_dot(s_learn, 218, 14, 6, COL_SHADOW);

    lv_obj_t *back = lv_obj_create(s_learn);
    lv_obj_set_pos(back, 10, 8);
    lv_obj_set_size(back, 28, 28);
    lv_obj_set_style_bg_color(back, lv_color_hex(COL_PILL), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_border_color(back, lv_color_hex(COL_PILL_BD), 0);
    lv_obj_set_style_pad_all(back, 0, 0);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *back_lab = lv_label_create(back);
    style_label(back_lab, &kids_font_20, COL_INK);
    lv_label_set_text(back_lab, "‹");
    lv_obj_align(back_lab, LV_ALIGN_CENTER, 2, -1);

    lv_obj_t *chip = lv_obj_create(s_learn);
    lv_obj_set_size(chip, 88, 24);
    lv_obj_align(chip, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(chip, lv_color_hex(COL_PILL), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip, 12, 0);
    lv_obj_set_style_border_width(chip, 2, 0);
    lv_obj_set_style_border_color(chip, lv_color_hex(COL_PILL_BD), 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    s_idx_lab = lv_label_create(chip);
    style_label(s_idx_lab, &kids_font_20, COL_INK);
    lv_label_set_text(s_idx_lab, "1/88");
    lv_obj_center(s_idx_lab);

    s_star_lab = lv_label_create(s_learn);
    style_label(s_star_lab, &kids_font_20, COL_MUTE);
    lv_label_set_text(s_star_lab, "☆");
    lv_obj_align(s_star_lab, LV_ALIGN_TOP_RIGHT, -14, 10);

    lv_obj_t *card = make_soft_card(s_learn, 10, 42, 220, 266);
    s_char_lab = lv_label_create(card);
    style_label(s_char_lab, &kids_font_56, COL_INK);
    lv_label_set_text(s_char_lab, "");
    lv_obj_align(s_char_lab, LV_ALIGN_CENTER, 0, 0);
}

static void build_quiz_pick(void)
{
    s_quiz_pick = make_screen(COL_BG);
    make_back_chip(s_quiz_pick);

    lv_obj_t *title = lv_label_create(s_quiz_pick);
    style_label(title, &kids_font_20, COL_ACCENT);
    lv_label_set_text(title, "测哪一册？");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < QUIZ_PICK_VISIBLE; ++i) {
        int y = 44 + i * 58;
        s_pick_row[i] = lv_obj_create(s_quiz_pick);
        lv_obj_set_pos(s_pick_row[i], 16, y);
        lv_obj_set_size(s_pick_row[i], 208, 52);
        lv_obj_set_style_bg_color(s_pick_row[i], lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_bg_opa(s_pick_row[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_pick_row[i], 16, 0);
        lv_obj_set_style_border_width(s_pick_row[i], 2, 0);
        lv_obj_set_style_border_color(s_pick_row[i], lv_color_hex(COL_EDGE), 0);
        lv_obj_set_style_pad_all(s_pick_row[i], 6, 0);
        lv_obj_clear_flag(s_pick_row[i], LV_OBJ_FLAG_SCROLLABLE);

        /* Single centered line — two labels overlapped (theme stretch + left text). */
        s_pick_title[i] = lv_label_create(s_pick_row[i]);
        style_label(s_pick_title[i], &kids_font_20, COL_INK);
        lv_obj_set_width(s_pick_title[i], LV_SIZE_CONTENT);
        lv_label_set_text(s_pick_title[i], "");
        lv_obj_align(s_pick_title[i], LV_ALIGN_CENTER, 0, 0);

        s_pick_stats[i] = lv_label_create(s_pick_row[i]);
        style_label(s_pick_stats[i], &kids_font_20, COL_MUTE);
        lv_label_set_text(s_pick_stats[i], "");
        lv_obj_add_flag(s_pick_stats[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *hint = lv_label_create(s_quiz_pick);
    style_label(hint, &kids_font_20, COL_MUTE);
    lv_label_set_text(hint, "短按确定开始 · 双击返回");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 224);

    make_cta(s_quiz_pick, 32, 250, 176, 44, "开始测验", NULL);
}

static void build_quiz(void)
{
    s_quiz = make_screen(COL_BG);
    lv_obj_t *wash = lv_obj_create(s_quiz);
    lv_obj_set_pos(wash, 0, 300);
    lv_obj_set_size(wash, 240, 20);
    lv_obj_set_style_bg_color(wash, lv_color_hex(COL_BG_DEEP), 0);
    lv_obj_set_style_bg_opa(wash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wash, 0, 0);
    lv_obj_set_style_radius(wash, 0, 0);
    lv_obj_clear_flag(wash, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pill = lv_obj_create(s_quiz);
    lv_obj_set_size(pill, 88, 28);
    lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(pill, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 14, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    s_quiz_prog_lab = lv_label_create(pill);
    style_label(s_quiz_prog_lab, &kids_font_20, COL_WHITE);
    lv_label_set_text(s_quiz_prog_lab, "1 / 88");
    lv_obj_center(s_quiz_prog_lab);

    for (int i = 0; i < 3; ++i) {
        int y = 40 + i * 86; /* h=74, gap=12 */
        s_quiz_opt_shadow[i] = lv_obj_create(s_quiz);
        lv_obj_set_pos(s_quiz_opt_shadow[i], 10, y + 4);
        lv_obj_set_size(s_quiz_opt_shadow[i], 220, 74);
        lv_obj_set_style_bg_color(s_quiz_opt_shadow[i], lv_color_hex(COL_SHADOW), 0);
        lv_obj_set_style_bg_opa(s_quiz_opt_shadow[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_quiz_opt_shadow[i], 22, 0);
        lv_obj_set_style_border_width(s_quiz_opt_shadow[i], 0, 0);
        lv_obj_clear_flag(s_quiz_opt_shadow[i], LV_OBJ_FLAG_SCROLLABLE);

        s_quiz_opt_card[i] = lv_obj_create(s_quiz);
        lv_obj_set_pos(s_quiz_opt_card[i], 10, y);
        lv_obj_set_size(s_quiz_opt_card[i], 220, 74);
        lv_obj_set_style_bg_color(s_quiz_opt_card[i], lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_bg_opa(s_quiz_opt_card[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_quiz_opt_card[i], 22, 0);
        lv_obj_set_style_border_width(s_quiz_opt_card[i], 2, 0);
        lv_obj_set_style_border_color(s_quiz_opt_card[i], lv_color_hex(COL_SHADOW), 0);
        lv_obj_set_style_pad_all(s_quiz_opt_card[i], 0, 0);
        lv_obj_clear_flag(s_quiz_opt_card[i], LV_OBJ_FLAG_SCROLLABLE);

        s_quiz_opt_lab[i] = lv_label_create(s_quiz_opt_card[i]);
        style_label(s_quiz_opt_lab[i], &kids_font_48, COL_INK);
        lv_label_set_text(s_quiz_opt_lab[i], "?");
        lv_obj_center(s_quiz_opt_lab[i]);
    }

}

static void build_thumbs_up(lv_obj_t *parent)
{
    /* Pink thumbs-up icon from assets/images/thumb_up.svg → RGB565. */
    s_res_thumb_box = lv_image_create(parent);
    lv_image_set_src(s_res_thumb_box, &thumb_up);
    lv_obj_align(s_res_thumb_box, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_add_flag(s_res_thumb_box, LV_OBJ_FLAG_HIDDEN);
}

static void build_result(void)
{
    s_result = make_screen(COL_BG);

    s_res_title = lv_label_create(s_result);
    style_label(s_res_title, &kids_font_32, COL_ACCENT);
    lv_label_set_text(s_res_title, "测完了");
    lv_obj_align(s_res_title, LV_ALIGN_TOP_MID, 0, 16);

    s_res_sub = lv_label_create(s_result);
    style_label(s_res_sub, &kids_font_32, COL_INK);
    lv_label_set_text(s_res_sub, "");
    lv_obj_align(s_res_sub, LV_ALIGN_TOP_MID, 0, 42);

    build_thumbs_up(s_result);

    s_res_stats = lv_label_create(s_result);
    style_label(s_res_stats, &kids_font_20, COL_MUTE);
    lv_label_set_text(s_res_stats, "");
    lv_obj_align(s_res_stats, LV_ALIGN_TOP_MID, 0, 150);

    s_res_wrong_card = make_soft_card(s_result, 16, 70, 208, 160);
    soft_card_set_hidden(s_res_wrong_card, true);
    s_res_wrong_lab = lv_label_create(s_res_wrong_card);
    style_label(s_res_wrong_lab, &kids_font_20, COL_ACCENT);
    lv_label_set_text(s_res_wrong_lab, "这些还要练");
    lv_obj_align(s_res_wrong_lab, LV_ALIGN_TOP_MID, 0, 4);

    for (int i = 0; i < RESULT_PAGE_SIZE; ++i) {
        int row = i / RESULT_GRID_COLS;
        int col = i % RESULT_GRID_COLS;
        int x = 8 + col * 48;
        int y = 32 + row * 48;
        s_res_grid_cell[i] = lv_obj_create(s_res_wrong_card);
        lv_obj_set_pos(s_res_grid_cell[i], x, y);
        lv_obj_set_size(s_res_grid_cell[i], 42, 42);
        lv_obj_set_style_bg_color(s_res_grid_cell[i], lv_color_hex(COL_PILL), 0);
        lv_obj_set_style_bg_opa(s_res_grid_cell[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_res_grid_cell[i], 10, 0);
        lv_obj_set_style_border_width(s_res_grid_cell[i], 2, 0);
        lv_obj_set_style_border_color(s_res_grid_cell[i], lv_color_hex(COL_PILL_BD), 0);
        lv_obj_set_style_pad_all(s_res_grid_cell[i], 0, 0);
        lv_obj_clear_flag(s_res_grid_cell[i], LV_OBJ_FLAG_SCROLLABLE);
        s_res_grid_lab[i] = lv_label_create(s_res_grid_cell[i]);
        style_label(s_res_grid_lab[i], &kids_font_20, COL_INK);
        lv_label_set_text(s_res_grid_lab[i], "");
        lv_obj_center(s_res_grid_lab[i]);
    }

    s_res_page_hint = lv_label_create(s_res_wrong_card);
    style_label(s_res_page_hint, &kids_font_20, COL_MUTE);
    lv_label_set_text(s_res_page_hint, "上下翻看");
    lv_obj_align(s_res_page_hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_add_flag(s_res_page_hint, LV_OBJ_FLAG_HIDDEN);

    /* Wider CTA so “双击确定返回主页” fits at kids_font_20. */
    make_cta(s_result, 16, 250, 208, 44, "双击确定返回主页", &s_res_cta_lab);
}


esp_err_t kids_ui_start(hanzi_progress_t *progress)
{
    if (!progress) return ESP_ERR_INVALID_ARG;
    s_prog = progress;
    s_home_slot = book_to_home_slot(s_prog->book);
    if (!bsp_lvgl_lock(1000)) return ESP_ERR_TIMEOUT;
    build_home();
    build_help();
    build_learn();
    build_quiz_pick();
    build_quiz();
    build_result();
    show_screen(KIDS_SCREEN_HOME);
    bsp_lvgl_unlock();
    ESP_LOGI(TAG, "kids UI ready (小儿识字 carousel + help + quiz flow)");
    return ESP_OK;
}

kids_screen_t kids_ui_screen(void)
{
    return s_screen;
}

static lv_obj_t *tip_parent_for_screen(void)
{
    switch (s_screen) {
    case KIDS_SCREEN_LEARN: return s_learn;
    case KIDS_SCREEN_HELP: return s_help;
    case KIDS_SCREEN_QUIZ_PICK: return s_quiz_pick;
    case KIDS_SCREEN_QUIZ: return s_quiz;
    case KIDS_SCREEN_RESULT: return s_result;
    default: return s_home;
    }
}

static bool handle_volume_long(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_LONG) return false;
    if (btn != BSP_BTN_UP && btn != BSP_BTN_DOWN) return false;

    uint8_t v = audio_worker_get_volume();
    if (btn == BSP_BTN_UP) {
        if (v < SETTINGS_VOLUME_MAX) {
            uint8_t next = (uint8_t)(v + SETTINGS_VOLUME_STEP);
            v = next > SETTINGS_VOLUME_MAX ? SETTINGS_VOLUME_MAX : next;
        }
    } else {
        if (v > SETTINGS_VOLUME_MIN) {
            int next = (int)v - SETTINGS_VOLUME_STEP;
            v = next < SETTINGS_VOLUME_MIN ? SETTINGS_VOLUME_MIN : (uint8_t)next;
        }
    }
    audio_worker_set_volume(v);
    char buf[24];
    snprintf(buf, sizeof(buf), "音量 %u", (unsigned)v);
    show_tip(tip_parent_for_screen(), buf);
    return true;
}

void kids_ui_on_button(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_prog) return;
    if (!bsp_lvgl_lock(200)) return;

    if (handle_volume_long(btn, ev)) {
        uint8_t vol = audio_worker_get_volume();
        bsp_lvgl_unlock();
        (void)settings_nvs_save_volume(vol);
        return;
    }

    bool need_persist = false;

    if (s_screen == KIDS_SCREEN_HOME) {
        if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            if (s_home_slot == HOME_SLOT_HELP) {
                show_screen(KIDS_SCREEN_HELP);
            } else if (s_home_slot == HOME_SLOT_QUIZ) {
                enter_quiz_pick();
            } else {
                enter_learn();
                need_persist = true;
            }
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            s_home_slot = (uint8_t)((s_home_slot + HOME_SLOT_COUNT - 1) % HOME_SLOT_COUNT);
            sync_book_from_home_slot();
            refresh_home();
            need_persist = home_slot_is_bookish(s_home_slot);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            s_home_slot = (uint8_t)((s_home_slot + 1) % HOME_SLOT_COUNT);
            sync_book_from_home_slot();
            refresh_home();
            need_persist = home_slot_is_bookish(s_home_slot);
        }
        /* long OK on home: no quiz entry (favorites toggle only on learn) */
    } else if (s_screen == KIDS_SCREEN_HELP) {
        if ((ev == BSP_BTN_CLICK || ev == BSP_BTN_DOUBLE) && btn == BSP_BTN_OK) {
            show_screen(KIDS_SCREEN_HOME);
        }
    } else if (s_screen == KIDS_SCREEN_LEARN) {
        if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
            s_home_slot = book_to_home_slot(s_prog->book);
            show_screen(KIDS_SCREEN_HOME);
            need_persist = true;
        } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
            uint16_t idx = cur_index();
            hanzi_progress_toggle_star(s_prog, idx);
            bool on = hanzi_progress_is_starred(s_prog, idx);
            refresh_learn();
            show_tip(s_learn, on ? "已收藏" : "已取消");
            need_persist = true;
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            bsp_lvgl_unlock();
            play_current();
            return;
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            hanzi_deck_session_prev(s_prog);
            refresh_learn();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            hanzi_deck_session_next(s_prog);
            refresh_learn();
        }
    } else if (s_screen == KIDS_SCREEN_QUIZ_PICK) {
        if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
            show_screen(KIDS_SCREEN_HOME);
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            if (s_pick_cursor > 0) s_pick_cursor--;
            refresh_quiz_pick();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            if (s_pick_cursor + 1 < QUIZ_PICK_COUNT) s_pick_cursor++;
            refresh_quiz_pick();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            uint8_t scope = (s_pick_cursor == 0) ? HANZI_DECK_FAVORITES : s_pick_cursor;
            if (hanzi_deck_is_favorites(scope) && hanzi_deck_favorite_count(s_prog) == 0) {
                show_tip(s_quiz_pick, "还没有收藏");
            } else {
                start_quiz_play(scope);
                need_persist = true;
            }
        }
    } else if (s_screen == KIDS_SCREEN_QUIZ) {
        if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
            show_quiz_result();
        } else if (!s_quiz_busy && ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            s_quiz_cursor = (uint8_t)((s_quiz_cursor + 2) % 3);
            refresh_quiz_opts();
        } else if (!s_quiz_busy && ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            s_quiz_cursor = (uint8_t)((s_quiz_cursor + 1) % 3);
            refresh_quiz_opts();
        } else if (!s_quiz_busy && ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            submit_quiz_answer();
        }
    } else if (s_screen == KIDS_SCREEN_RESULT) {
        if ((ev == BSP_BTN_CLICK || ev == BSP_BTN_DOUBLE) && btn == BSP_BTN_OK) {
            s_home_slot = book_to_home_slot(s_quiz_scope_book);
            sync_book_from_home_slot();
            show_screen(KIDS_SCREEN_HOME);
            need_persist = true;
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            if (s_quiz_wrong_count > RESULT_PAGE_SIZE && s_res_wrong_page > 0) {
                s_res_wrong_page--;
                refresh_result();
            }
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            uint16_t pages = s_quiz_wrong_count
                ? (uint16_t)((s_quiz_wrong_count + RESULT_PAGE_SIZE - 1) / RESULT_PAGE_SIZE)
                : 1;
            if (s_res_wrong_page + 1 < pages) {
                s_res_wrong_page++;
                refresh_result();
            }
        }
    }

    bsp_lvgl_unlock();
    if (need_persist) {
        persist();
    }
}
