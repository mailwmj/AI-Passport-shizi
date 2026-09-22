# 小儿识字（Xiaoer-Shizi）

[English](README.md)

基于 [FoloToy AI Passport](https://github.com/folotoy/ai-passport)（ESP32-C3）的儿童识字固件。

粉嫩 UI，课程为 **四五快读第 1～7 册**（960 字槽），离线读音、收藏、听音选字测验。

本仓库是成品应用，不是上游硬件 demo 菜单。

## 界面预览

<p align="center">
  <img src="assets/screenshots/home_quiz.png" alt="主页：开始测验" width="180" height="249">
  &nbsp;
  <img src="assets/screenshots/home_favorites.png" alt="主页：我的收藏" width="180" height="249">
  &nbsp;
  <img src="assets/screenshots/home_book1.png" alt="主页：第 1 册" width="180" height="249">
</p>
<p align="center">
  <img src="assets/screenshots/learn.png" alt="学习页" width="180" height="249">
  &nbsp;
  <img src="assets/screenshots/quiz.png" alt="测验页" width="180" height="249">
</p>
<p align="center">
  <img src="assets/screenshots/result_ok.png" alt="结果：全对" width="180" height="249">
  &nbsp;
  <img src="assets/screenshots/result_mid.png" alt="结果：中途退出" width="180" height="249">
</p>

## 功能

- **主页轮播（10 格）**：怎么用 → 开始测验 → 我的收藏 → 第 1～7 册；中间卡标题用中号字（约 32px），标题与星标数成组居中
- **怎么用**：全屏按键说明
- **学习**：约 88px 大字；短按确定听读音；长按确定收藏；双击确定返回
- **测验**：
  - 先选册或收藏（一行：`第 N 册 · N字`）
  - 听音三选一（约 48px）；当前选项为实心粉底白字
  - **答错**：只记第一次对错；立刻重读该字并打乱选项，留在本题直到选对再进入下一题
  - **结果**：中途退出显示「先到这里」+ 对/错；测完有错列出错字；全对显示「真棒！」与点赞图标；底部按钮文案为「双击确定返回主页」
- **音量**：任意页长按上/下 ±10（10–100，默认 90），写入 NVS
- **进度**：收藏与选册保存在 NVS

## 按键口诀

| 按键 | 作用 |
| --- | --- |
| 上 / 下（短按） | 换选（卡 / 字 / 选项 / 错字翻页） |
| 上 / 下（长按） | 音量 |
| 确定（短按） | 主操作（打开 / 听 / 交卷 / 关闭说明或结果） |
| 确定（双击） | 返回上一级（测验中为退出并看成绩） |
| 确定（长按） | 仅学习页：收藏 |

## 字表与语音（四五快读 1～7 册）

| 册 | 约字数 | 说明 |
| --- | ---: | --- |
| 第 1～4 册 | 各 88 | 出版社统计口径 |
| 第 5 册 | 89 | 同上 |
| 第 6 册 | 111 | 同上 |
| 第 7 册 | 408 | 字族拓展重构（非原书页序 OCR） |
| **合计** | **960** | |

数据：`main/data/siwu_hanzi_books1to7.*`（来源见 `main/data/PROVENANCE.zh_CN.md`）。语音包 `assets/voice/hanzi_voice_pack.bin`，晓晓音色，960/960。

## 编译与刷机

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
```

将 `build/Xiaoer-Shizi-full.bin` 从 `0x0` 整包写入（合并镜像可能重置 NVS）。

## 主要代码

| 路径 | 作用 |
| --- | --- |
| `main/main.c` | 入口 |
| `main/ui/kids_ui.c` | 主页 / 说明 / 学习 / 测验 / 结果 |
| `main/hanzi/` | 字表、牌组、语音、NVS |
| `assets/fonts/` | `kids_font_20/32/48/56`（56 名为 ABI，学习页实际约 88px） |
| `assets/images/thumb_up.*` | 全对结果页点赞图标（SVG → RGB565） |
| `assets/voice/` · `scripts/` | 语音与生成脚本 |

硬件与上游说明见 `docs/`、`AGENTS.zh_CN.md`。许可证：`LICENSE`（MIT，基于 FoloToy AI Passport）。
