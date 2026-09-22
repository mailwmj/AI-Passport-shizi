#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hanzi_deck.h"

int main(void)
{
    assert(SIWU_HANZI_COUNT == 960);
    assert(SIWU_BOOK_COUNT == 7);
    assert(g_siwu_book_counts[0] == 88);
    assert(g_siwu_book_counts[5] == 111);
    assert(g_siwu_book_counts[6] == 408);
    assert(SIWU_MAX_BOOK_CHARS >= 408);

    const siwu_hanzi_t *h0 = siwu_hanzi_get(0);
    assert(h0 && h0->index_global == 1 && h0->book == 1);

    const siwu_hanzi_t *h7 = siwu_hanzi_get(552);
    assert(h7 && h7->book == 7 && h7->index_in_book == 1);

    hanzi_progress_t p;
    hanzi_progress_init(&p);
    assert(p.book == 1 && p.mastered_count == 0);
    assert(!hanzi_progress_is_starred(&p, 0));

    hanzi_progress_toggle_star(&p, 0);
    assert(hanzi_progress_is_starred(&p, 0));
    assert(p.mastered_count == 1);

    hanzi_deck_enter_book(&p, 1, 42);
    assert(p.session_len == 88);
    assert(hanzi_progress_is_starred(&p, 0));

    hanzi_deck_enter_book(&p, 1, 1);
    uint16_t order_a[88];
    memcpy(order_a, p.session_order, sizeof(order_a));
    hanzi_deck_enter_book(&p, 1, 2);
    assert(memcmp(order_a, p.session_order, sizeof(order_a)) != 0);

    p.session_pos = 0;
    uint16_t cur = hanzi_deck_current(&p);
    hanzi_deck_session_next(&p);
    assert(hanzi_deck_current(&p) != cur || p.session_len == 1);
    hanzi_deck_session_prev(&p);
    assert(hanzi_deck_current(&p) == cur);

    assert(hanzi_deck_next_book(7) == HANZI_DECK_FAVORITES);
    assert(hanzi_deck_next_book(HANZI_DECK_FAVORITES) == 1);
    assert(hanzi_deck_prev_book(1) == HANZI_DECK_FAVORITES);
    assert(hanzi_deck_prev_book(HANZI_DECK_FAVORITES) == 7);
    assert(hanzi_deck_book_of(0) == 1);
    assert(hanzi_deck_book_of(88) == 2);
    assert(hanzi_deck_book_of(552) == 7);

    hanzi_deck_enter_book(&p, 7, 99);
    assert(p.session_len == 408);
    assert(hanzi_deck_book_of(hanzi_deck_current(&p)) == 7);

    hanzi_progress_t fav;
    hanzi_progress_init(&fav);
    assert(!hanzi_deck_enter_favorites(&fav, 1));
    assert(fav.session_len == 0);
    hanzi_progress_set_starred(&fav, 10, true);
    hanzi_progress_set_starred(&fav, 200, true);
    hanzi_progress_set_starred(&fav, 900, true);
    assert(hanzi_deck_favorite_count(&fav) == 3);
    assert(hanzi_deck_enter_favorites(&fav, 11));
    assert(fav.book == HANZI_DECK_FAVORITES);
    assert(fav.session_len == 3);
    assert(hanzi_deck_enter_favorites(&fav, 22));
    assert(fav.session_len == 3);
    for (int i = 0; i < 3; ++i) {
        assert(hanzi_progress_is_starred(&fav, fav.session_order[i]));
    }

    uint16_t opts[3];
    hanzi_quiz_options(5, opts, 12345);
    int found = 0;
    for (int i = 0; i < 3; ++i) {
        assert(opts[i] < SIWU_HANZI_COUNT);
        if (opts[i] == 5) found = 1;
    }
    assert(found);

    printf("test_hanzi_deck: PASS (shuffle + favorites + books1-7)\n");
    return 0;
}
