#!/usr/bin/env python3
"""Generate C character table from siwu_hanzi_books1to7.json."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JSON_PATH = ROOT / "main/data/siwu_hanzi_books1to7.json"
OUT_H = ROOT / "main/hanzi/siwu_hanzi_table.h"
OUT_C = ROOT / "main/hanzi/siwu_hanzi_table.c"


def c_escape(s: str) -> str:
    return s.encode("unicode_escape").decode("ascii").replace('"', '\\"')


def main() -> int:
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    chars = data["characters"]
    assert len(chars) == data["row_count"]

    book_nums = sorted({int(r["book"]) for r in chars})
    book_count = max(book_nums)
    book_counts = [0] * book_count
    for row in chars:
        book_counts[int(row["book"]) - 1] += 1
    max_book = max(book_counts) if book_counts else 0

    ui_extra = (
        "小儿识字怎么用开始测验去测验查看说明测哪一册测完了真棒全对啦"
        "这些还要练我知道了退出测验对错短按确定长按双击返回"
        "上下换卡换字换选项听读音选答案打开收藏仅学习页调音量"
        "先选册或收藏回主页翻看答对啦再听听还没有收藏已取消音量"
        "开始学习已学册第课恭喜掌握挑选择上下确定"
    )
    pinyin_extra = (
        "abcdefghijklmnopqrstuvwxyz"
        "āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜü"
        "ĀÁǍÀĒÉĚÈĪÍǏÌŌÓǑÒŪÚǓÙǕǗǙǛÜ"
    )
    unique = []
    seen = set()
    for row in chars:
        ch = row["char"]
        if ch not in seen:
            seen.add(ch)
            unique.append(ch)
        for ch in (row.get("pinyin") or ""):
            if ch not in seen:
                seen.add(ch)
                unique.append(ch)
    for ch in ui_extra:
        if ch not in seen and ("\u4e00" <= ch <= "\u9fff" or ch in pinyin_extra):
            seen.add(ch)
            unique.append(ch)

    symbols_path = ROOT / "assets/fonts/kids_symbols.txt"
    symbols_path.parent.mkdir(parents=True, exist_ok=True)
    ascii_part = "".join(chr(c) for c in range(0x20, 0x7F))
    punct = "，。！？、：；「」『』（）【】《》—…·★☆♥•♪‹›/0123456789%"
    blob = ascii_part + "".join(unique) + punct + ui_extra + pinyin_extra
    ordered = []
    seen2 = set()
    for ch in blob:
        if ch not in seen2:
            seen2.add(ch)
            ordered.append(ch)
    symbols_path.write_text("".join(ordered), encoding="utf-8")

    lines_h = [
        "#pragma once",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        "#define SIWU_HANZI_COUNT %d" % len(chars),
        "#define SIWU_HANZI_UNIQUE %d" % data["unique_count"],
        "#define SIWU_BOOK_COUNT %d" % book_count,
        "#define SIWU_MAX_BOOK_CHARS %d" % max_book,
        "",
        "typedef struct {",
        "    uint16_t index_global; /* 1-based */",
        "    uint8_t book;          /* 1..SIWU_BOOK_COUNT */",
        "    uint8_t index_in_book; /* 1-based */",
        "    uint16_t codepoint;",
        "    const char *utf8;      /* single UTF-8 character */",
        "    const char *pinyin;",
        "} siwu_hanzi_t;",
        "",
        "extern const siwu_hanzi_t g_siwu_hanzi[SIWU_HANZI_COUNT];",
        "extern const uint16_t g_siwu_book_counts[SIWU_BOOK_COUNT];",
        "",
        "const siwu_hanzi_t *siwu_hanzi_get(uint16_t zero_based_index);",
        "uint16_t siwu_hanzi_book_start(uint8_t book); /* 0-based index of first char in book */",
        "",
    ]
    OUT_H.write_text("\n".join(lines_h), encoding="utf-8")

    rows_c = []
    for row in chars:
        book = int(row["book"])
        cp = int(row["unicode"].replace("U+", ""), 16)
        utf8 = row["char"]
        utf8_esc = "".join("\\x%02x" % b for b in utf8.encode("utf-8"))
        py = row.get("pinyin") or ""
        py_esc = c_escape(py)
        # index_in_book may exceed uint8 for book7 (408) — widen field if needed
        iib = int(row["index_in_book"])
        rows_c.append(
            "    { %d, %d, %d, 0x%04X, \"%s\", \"%s\" }"
            % (row["index_global"], book, iib, cp, utf8_esc, py_esc)
        )

    # index_in_book is uint8_t but book7 has 408 — MUST widen to uint16_t
    lines_h2 = [
        "#pragma once",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        "#define SIWU_HANZI_COUNT %d" % len(chars),
        "#define SIWU_HANZI_UNIQUE %d" % data["unique_count"],
        "#define SIWU_BOOK_COUNT %d" % book_count,
        "#define SIWU_MAX_BOOK_CHARS %d" % max_book,
        "",
        "typedef struct {",
        "    uint16_t index_global; /* 1-based */",
        "    uint8_t book;          /* 1..SIWU_BOOK_COUNT */",
        "    uint16_t index_in_book; /* 1-based */",
        "    uint16_t codepoint;",
        "    const char *utf8;      /* single UTF-8 character */",
        "    const char *pinyin;",
        "} siwu_hanzi_t;",
        "",
        "extern const siwu_hanzi_t g_siwu_hanzi[SIWU_HANZI_COUNT];",
        "extern const uint16_t g_siwu_book_counts[SIWU_BOOK_COUNT];",
        "",
        "const siwu_hanzi_t *siwu_hanzi_get(uint16_t zero_based_index);",
        "uint16_t siwu_hanzi_book_start(uint8_t book); /* 0-based index of first char in book */",
        "",
    ]
    OUT_H.write_text("\n".join(lines_h2), encoding="utf-8")

    lines_c = [
        '#include "siwu_hanzi_table.h"',
        "",
        "const uint16_t g_siwu_book_counts[SIWU_BOOK_COUNT] = { %s };"
        % ", ".join(str(c) for c in book_counts),
        "",
        "const siwu_hanzi_t g_siwu_hanzi[SIWU_HANZI_COUNT] = {",
        ",\n".join(rows_c),
        "};",
        "",
        "const siwu_hanzi_t *siwu_hanzi_get(uint16_t zero_based_index)",
        "{",
        "    if (zero_based_index >= SIWU_HANZI_COUNT) return 0;",
        "    return &g_siwu_hanzi[zero_based_index];",
        "}",
        "",
        "uint16_t siwu_hanzi_book_start(uint8_t book)",
        "{",
        "    if (book < 1 || book > SIWU_BOOK_COUNT) return 0;",
        "    uint16_t start = 0;",
        "    for (uint8_t b = 1; b < book; ++b) start = (uint16_t)(start + g_siwu_book_counts[b - 1]);",
        "    return start;",
        "}",
        "",
    ]
    OUT_C.write_text("\n".join(lines_c), encoding="utf-8")
    print(f"Wrote {OUT_H.relative_to(ROOT)} and {OUT_C.relative_to(ROOT)} ({len(chars)} rows, {book_count} books, max/book={max_book})")
    print(f"Wrote {symbols_path.relative_to(ROOT)} ({len(symbols_path.read_text('utf-8'))} chars)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
