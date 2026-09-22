#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "siwu_hanzi_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* book==HANZI_DECK_FAVORITES means 「我的收藏」 picker slot (not a SIWU book). */
#define HANZI_DECK_FAVORITES 0

typedef struct {
    uint8_t book;            /* 0=favorites, or 1..SIWU_BOOK_COUNT */
    uint16_t session_pos;    /* 0-based into session_order */
    uint16_t session_len;    /* chars in current shuffled session (0 = not entered) */
    uint16_t session_order[SIWU_HANZI_COUNT]; /* global 0-based indices (favorites may span all) */
    uint16_t mastered_count; /* number of favorited chars (all books); legacy name kept */
    /* Bitset: one bit per curriculum slot (stable by character id / global index) = 收藏 */
    uint8_t stars[(SIWU_HANZI_COUNT + 7) / 8];
} hanzi_progress_t;

void hanzi_progress_init(hanzi_progress_t *p);
bool hanzi_progress_is_starred(const hanzi_progress_t *p, uint16_t index);
void hanzi_progress_set_starred(hanzi_progress_t *p, uint16_t index, bool starred);
void hanzi_progress_toggle_star(hanzi_progress_t *p, uint16_t index);
void hanzi_progress_recount(hanzi_progress_t *p);

/* Book / favorites selection on home (no shuffle yet). */
void hanzi_deck_select_book(hanzi_progress_t *p, uint8_t book);
uint8_t hanzi_deck_next_book(uint8_t book); /* cycles 1..7, favorites(0) */
uint8_t hanzi_deck_prev_book(uint8_t book);
bool hanzi_deck_is_favorites(uint8_t book);

/* Enter learn: reshuffle that book's characters every time. seed!=0 preferred. */
void hanzi_deck_enter_book(hanzi_progress_t *p, uint8_t book, uint32_t seed);

/* Enter 收藏: only favorited chars, reshuffled. Returns false if empty (session unchanged). */
bool hanzi_deck_enter_favorites(hanzi_progress_t *p, uint32_t seed);

uint16_t hanzi_deck_current(const hanzi_progress_t *p); /* global index */
void hanzi_deck_session_next(hanzi_progress_t *p);
void hanzi_deck_session_prev(hanzi_progress_t *p);

uint8_t hanzi_deck_book_of(uint16_t index);
uint16_t hanzi_deck_learned_in_book(const hanzi_progress_t *p, uint8_t book);
uint16_t hanzi_deck_favorite_count(const hanzi_progress_t *p);

/* Quiz: fill 3 option indices (global), prefer same book as correct. */
void hanzi_quiz_options(uint16_t correct_index, uint16_t options_out[3], uint32_t seed);

#ifdef __cplusplus
}
#endif
