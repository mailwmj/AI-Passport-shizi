#!/usr/bin/env bash
# Generate LVGL subset fonts for the kids 认字 app.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"

FONT_SRC="${KIDS_FONT_SRC:-}"
if [[ -z "$FONT_SRC" ]]; then
  for candidate in \
    "assets/fonts/SourceHanSansSC-Normal.otf" \
    $HOME/.cache/Espressif/ComponentManager/*/lvgl__lvgl_*/scripts/built_in_font/SourceHanSansSC-Normal.otf
  do
    if [[ -f "$candidate" ]]; then
      FONT_SRC="$candidate"
      break
    fi
  done
fi
if [[ -z "${FONT_SRC}" || ! -f "$FONT_SRC" ]]; then
  echo "ERROR: Source Han Sans SC Normal OTF not found. Set KIDS_FONT_SRC." >&2
  exit 1
fi

if [[ ! -f assets/fonts/kids_symbols.txt ]]; then
  python3 scripts/gen_hanzi_table.py
fi

python3 - <<'PY'
from pathlib import Path
s = Path("assets/fonts/kids_symbols.txt").read_text(encoding="utf-8")
big = "".join(sorted(set(ch for ch in s if ("\u4e00" <= ch <= "\u9fff") or ch in "0123456789★☆")))
Path("assets/fonts/kids_symbols_big.txt").write_text(big, encoding="utf-8")
print(f"big symbols: {len(big)}")
PY

# Mid UI titles (home center card) — keep glyph set small.
python3 - <<'PY'
from pathlib import Path
needed = "0123456789 第册开始测验我的收藏说明听选先选册或收藏真棒★☆‹›·/先到这里对错测完了真棒全对啦！"
seen = set()
out = []
for ch in needed:
    if ch not in seen:
        seen.add(ch)
        out.append(ch)
Path("assets/fonts/kids_symbols_mid.txt").write_text("".join(out), encoding="utf-8")
print(f"mid symbols: {len(out)}")
PY

command -v lv_font_conv >/dev/null || { echo "ERROR: lv_font_conv missing" >&2; exit 1; }
mkdir -p assets/fonts
echo "Using font: $FONT_SRC"

# Symbol name remains kids_font_56 for stable C references; Learn glyph size is 88px bpp2.
lv_font_conv \
  --font "$FONT_SRC" \
  --symbols "$(cat assets/fonts/kids_symbols_big.txt)" \
  --size 88 --bpp 2 --format lvgl --no-compress \
  --lv-font-name kids_font_56 --lv-include lvgl.h \
  --output assets/fonts/kids_font_56.c

# Quiz option glyphs — slightly smaller so they don't hug taller pills.
lv_font_conv \
  --font "$FONT_SRC" \
  --symbols "$(cat assets/fonts/kids_symbols_big.txt)" \
  --size 48 --bpp 2 --format lvgl --no-compress \
  --lv-font-name kids_font_48 --lv-include lvgl.h \
  --output assets/fonts/kids_font_48.c

# Home center titles (~32px); peeks stay on kids_font_20.
lv_font_conv \
  --font "$FONT_SRC" \
  --symbols "$(cat assets/fonts/kids_symbols_mid.txt)" \
  --size 32 --bpp 2 --format lvgl --no-compress \
  --lv-font-name kids_font_32 --lv-include lvgl.h \
  --output assets/fonts/kids_font_32.c

lv_font_conv \
  --font "$FONT_SRC" \
  --symbols "$(cat assets/fonts/kids_symbols.txt)" \
  --size 18 --bpp 2 --format lvgl --no-compress \
  --lv-font-name kids_font_20 --lv-include lvgl.h \
  --output assets/fonts/kids_font_20.c

echo "Keeping assets/fonts/README_kids.md (+ zh_CN) as tracked docs"
ls -lh assets/fonts/kids_font_*.c
