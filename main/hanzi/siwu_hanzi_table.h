#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SIWU_HANZI_COUNT 960
#define SIWU_HANZI_UNIQUE 956
#define SIWU_BOOK_COUNT 7
#define SIWU_MAX_BOOK_CHARS 408

typedef struct {
    uint16_t index_global; /* 1-based */
    uint8_t book;          /* 1..SIWU_BOOK_COUNT */
    uint16_t index_in_book; /* 1-based */
    uint16_t codepoint;
    const char *utf8;      /* single UTF-8 character */
    const char *pinyin;
} siwu_hanzi_t;

extern const siwu_hanzi_t g_siwu_hanzi[SIWU_HANZI_COUNT];
extern const uint16_t g_siwu_book_counts[SIWU_BOOK_COUNT];

const siwu_hanzi_t *siwu_hanzi_get(uint16_t zero_based_index);
uint16_t siwu_hanzi_book_start(uint8_t book); /* 0-based index of first char in book */
