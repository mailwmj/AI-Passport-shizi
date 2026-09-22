#include "hanzi_deck.h"

void hanzi_progress_init(hanzi_progress_t *p)
{
    if (!p) return;
    p->book = 1;
    p->session_pos = 0;
    p->session_len = 0;
    p->mastered_count = 0;
    for (unsigned i = 0; i < sizeof(p->stars); ++i) p->stars[i] = 0;
    for (unsigned i = 0; i < SIWU_HANZI_COUNT; ++i) p->session_order[i] = 0;
}

bool hanzi_progress_is_starred(const hanzi_progress_t *p, uint16_t index)
{
    if (!p || index >= SIWU_HANZI_COUNT) return false;
    return (p->stars[index / 8] & (uint8_t)(1u << (index % 8))) != 0;
}

void hanzi_progress_recount(hanzi_progress_t *p)
{
    if (!p) return;
    uint16_t n = 0;
    for (uint16_t i = 0; i < SIWU_HANZI_COUNT; ++i) {
        if (hanzi_progress_is_starred(p, i)) ++n;
    }
    p->mastered_count = n;
}

void hanzi_progress_set_starred(hanzi_progress_t *p, uint16_t index, bool starred)
{
    if (!p || index >= SIWU_HANZI_COUNT) return;
    uint8_t mask = (uint8_t)(1u << (index % 8));
    if (starred) p->stars[index / 8] |= mask;
    else p->stars[index / 8] &= (uint8_t)~mask;
    hanzi_progress_recount(p);
}

void hanzi_progress_toggle_star(hanzi_progress_t *p, uint16_t index)
{
    hanzi_progress_set_starred(p, index, !hanzi_progress_is_starred(p, index));
}

bool hanzi_deck_is_favorites(uint8_t book)
{
    return book == HANZI_DECK_FAVORITES;
}

void hanzi_deck_select_book(hanzi_progress_t *p, uint8_t book)
{
    if (!p) return;
    if (book != HANZI_DECK_FAVORITES) {
        if (book < 1) book = 1;
        if (book > SIWU_BOOK_COUNT) book = SIWU_BOOK_COUNT;
    }
    p->book = book;
}

uint8_t hanzi_deck_next_book(uint8_t book)
{
    /* Cycle: 1→2→…→7→favorites(0)→1 */
    if (hanzi_deck_is_favorites(book)) return 1;
    if (book < 1) return 1;
    if (book >= SIWU_BOOK_COUNT) return HANZI_DECK_FAVORITES;
    return (uint8_t)(book + 1);
}

uint8_t hanzi_deck_prev_book(uint8_t book)
{
    /* Cycle: 1←favorites(0)←7←…←2←1 */
    if (hanzi_deck_is_favorites(book)) return SIWU_BOOK_COUNT;
    if (book <= 1) return HANZI_DECK_FAVORITES;
    return (uint8_t)(book - 1);
}

static uint32_t lcg(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void shuffle_session(hanzi_progress_t *p, uint32_t seed)
{
    uint16_t count = p->session_len;
    uint32_t state = seed ? seed : 1u;
    for (int i = (int)count - 1; i > 0; --i) {
        int j = (int)(lcg(&state) % (uint32_t)(i + 1));
        uint16_t tmp = p->session_order[i];
        p->session_order[i] = p->session_order[j];
        p->session_order[j] = tmp;
    }
}

void hanzi_deck_enter_book(hanzi_progress_t *p, uint8_t book, uint32_t seed)
{
    if (!p) return;
    if (hanzi_deck_is_favorites(book)) {
        (void)hanzi_deck_enter_favorites(p, seed);
        return;
    }
    hanzi_deck_select_book(p, book);
    book = p->book;
    uint16_t start = siwu_hanzi_book_start(book);
    uint16_t count = g_siwu_book_counts[book - 1];
    if (count > SIWU_HANZI_COUNT) count = SIWU_HANZI_COUNT;
    p->session_len = count;
    p->session_pos = 0;
    for (uint16_t i = 0; i < count; ++i) {
        p->session_order[i] = (uint16_t)(start + i);
    }
    shuffle_session(p, seed ? seed : (uint32_t)book * 2654435761u + 1u);
}

bool hanzi_deck_enter_favorites(hanzi_progress_t *p, uint32_t seed)
{
    if (!p) return false;
    uint16_t n = 0;
    for (uint16_t i = 0; i < SIWU_HANZI_COUNT; ++i) {
        if (hanzi_progress_is_starred(p, i)) {
            p->session_order[n++] = i;
        }
    }
    if (n == 0) {
        /* Leave prior session alone; caller stays on picker. */
        return false;
    }
    p->book = HANZI_DECK_FAVORITES;
    p->session_len = n;
    p->session_pos = 0;
    shuffle_session(p, seed ? seed : 0xFA17u);
    return true;
}

uint16_t hanzi_deck_current(const hanzi_progress_t *p)
{
    if (!p || p->session_len == 0) {
        if (p && hanzi_deck_is_favorites(p->book)) return 0;
        return siwu_hanzi_book_start(p ? p->book : 1);
    }
    if (p->session_pos >= p->session_len) return p->session_order[0];
    return p->session_order[p->session_pos];
}

void hanzi_deck_session_next(hanzi_progress_t *p)
{
    if (!p || p->session_len == 0) return;
    p->session_pos = (uint16_t)((p->session_pos + 1u) % p->session_len);
}

void hanzi_deck_session_prev(hanzi_progress_t *p)
{
    if (!p || p->session_len == 0) return;
    p->session_pos = (uint16_t)((p->session_pos + p->session_len - 1u) % p->session_len);
}

uint8_t hanzi_deck_book_of(uint16_t index)
{
    const siwu_hanzi_t *h = siwu_hanzi_get(index);
    return h ? h->book : 1;
}

uint16_t hanzi_deck_favorite_count(const hanzi_progress_t *p)
{
    return p ? p->mastered_count : 0;
}

uint16_t hanzi_deck_learned_in_book(const hanzi_progress_t *p, uint8_t book)
{
    if (!p) return 0;
    if (hanzi_deck_is_favorites(book)) return p->mastered_count;
    if (book < 1 || book > SIWU_BOOK_COUNT) return 0;
    uint16_t start = siwu_hanzi_book_start(book);
    uint16_t count = g_siwu_book_counts[book - 1];
    uint16_t n = 0;
    for (uint16_t i = 0; i < count; ++i) {
        if (hanzi_progress_is_starred(p, (uint16_t)(start + i))) ++n;
    }
    return n;
}

void hanzi_quiz_options(uint16_t correct_index, uint16_t options_out[3], uint32_t seed)
{
    if (!options_out || SIWU_HANZI_COUNT < 3) {
        if (options_out) {
            options_out[0] = correct_index;
            options_out[1] = correct_index;
            options_out[2] = correct_index;
        }
        return;
    }
    uint32_t state = seed ? seed : (uint32_t)correct_index * 2654435761u + 1u;
    options_out[0] = correct_index;
    uint8_t book = hanzi_deck_book_of(correct_index);
    uint16_t start = siwu_hanzi_book_start(book);
    uint16_t count = g_siwu_book_counts[book - 1];
    for (int slot = 1; slot < 3; ++slot) {
        uint16_t cand;
        int guard = 0;
        do {
            if (count >= 3) {
                cand = (uint16_t)(start + (lcg(&state) % count));
            } else {
                cand = (uint16_t)(lcg(&state) % SIWU_HANZI_COUNT);
            }
            ++guard;
        } while (guard < 64 && (cand == options_out[0] || (slot == 2 && cand == options_out[1])));
        options_out[slot] = cand;
    }
    for (int i = 2; i > 0; --i) {
        int j = (int)(lcg(&state) % (uint32_t)(i + 1));
        uint16_t tmp = options_out[i];
        options_out[i] = options_out[j];
        options_out[j] = tmp;
    }
}
