<p align="right">
  <strong>简体中文</strong> · <a href="README_kids.md">English</a>
</p>

# 儿童 LVGL 子集字体

由 `scripts/gen_kids_fonts.sh` 从思源黑体 SC Normal 生成。

| 符号 | 字号 | 用途 |
| --- | --- | --- |
| `kids_font_56` | **88px** bpp2 | Learn 大汉字（ABI 名保留） |
| `kids_font_48` | **48px** bpp2 | Quiz 选项字形（高 pill 内留边） |
| `kids_font_20` | 18px bpp2 | UI 文案 |

字形清单：`kids_symbols.txt` / `kids_symbols_big.txt`（1–7 册 + 收藏 UI）。

88px Learn 字体需开启 `CONFIG_LV_FONT_FMT_TXT_LARGE=y`。
