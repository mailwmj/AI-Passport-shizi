**English** · [简体中文](PROVENANCE.zh_CN.md)

# Provenance: Siwu Fast-Read Books 1–7 Character Table

## Files
- `siwu_hanzi_books1to7.json` — full inventory and metadata (primary)
- `siwu_hanzi_books1to7.csv` — flat table for tooling
- `siwu_hanzi_books1to6.json` / `.csv` — archived Books 1–6 subset (552)

## Scope
Books **1–7** toward the commonly cited full-set size of **~960** curriculum slots:
- Books 1–6: publisher totals **552** (88+88+88+88+89+111)
- Book 7: **phonetic-family (zifazu) extension** reconstruction **408** slots (best-effort; not OCR of licensed print)

## Sources (public, best-effort)
1. Hunan Science & Technology Press product page — per-book new-character counts for 1–6.
2. Aggregated community character summary text (public mirror) for Books 1–6.
3. Xiaohuasheng Book 6 lesson checklists — restore the four reintroduced glyphs
   (U+8FD8, U+89C9, U+7740, U+4E86) into Book 6 and correct U+4FAF → U+5019.
4. Public Book 1 scan notes — early-order cross-check.
5. Community pedagogy notes: Book 7 expands ~400 glyphs via phonetic families from components in 1–6;
   full set often cited as ~960 including Book 7 extensions.
6. [CJKVI IDS](https://github.com/cjkvi/cjkvi-ids) — component trees for family expansion.
7. [General Standard Chinese Characters Level-1 table](https://github.com/ben-hua/general_standard_chinese) — frequency order for picking family members.

## Book 7 method
For each productive component already present in Books 1–6, collect Level-1 characters that
contain that component and are **not** already in 1–6, ranked by frequency, until **408**
new slots. Order within Book 7 is frequency-within-family, **not** publisher page order.

## Known gaps / verification debt
- Not OCR'd from licensed print PDFs; community aggregates and IDS reconstruction can drift.
- Books 1–5 lack per-lesson IDs (only Book 6 mapped to lessons 51–60).
- Four Book 6 slots reintroduce earlier glyphs; those rows are marked `reintroduced`.
- Book 7 is pedagogical reconstruction — glyph set aims at ~960 total, not a page facsimile.
- Polyphonic characters: `pinyin` is a single automated guess (pypinyin).

## License note
Character lists are educational metadata reconstructed from public descriptions and open
component databases. Do not commit copyrighted book scans or publisher artwork into this repository.
