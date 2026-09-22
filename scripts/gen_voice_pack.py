#!/usr/bin/env python3
"""Generate compressed IMA-ADPCM voice pack for 四五快读 characters.

Requires: edge-tts, ffmpeg. Network needed for Microsoft Edge TTS voices.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JSON_PATH = ROOT / "main/data/siwu_hanzi_books1to7.json"
OUT_BIN = ROOT / "assets/voice/hanzi_voice_pack.bin"
OUT_META = ROOT / "assets/voice/VOICE_COVERAGE.md"
OUT_HDR = ROOT / "main/hanzi/voice_pack_meta.h"
WORK = ROOT / "assets/voice/_work"

# IMA-ADPCM step/index tables (standard)
IMA_STEP = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
]
IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def encode_ima_adpcm(samples: list[int]) -> bytes:
    """Encode mono int16 PCM to IMA-ADPCM (4-bit), with 4-byte block header per 505 samples? 

    We use a simple continuous stream format:
      [int16 predictor][uint8 step_index][uint8 pad] then nibbles packed low-first.
    Compatible with our on-device decoder in voice_adpcm.c.
    """
    if not samples:
        return b""
    pred = 0
    index = 0
    out = bytearray()
    out += struct.pack("<hBB", pred, index, 0)
    nibble_buf = None
    for s in samples:
        step = IMA_STEP[index]
        diff = s - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        if diff >= step:
            code |= 4
            diff -= step
        step_h = step >> 1
        if diff >= step_h:
            code |= 2
            diff -= step_h
        step_q = step_h >> 1
        if diff >= step_q:
            code |= 1

        # reconstruct
        diffq = step >> 3
        if code & 4:
            diffq += step
        if code & 2:
            diffq += step >> 1
        if code & 1:
            diffq += step >> 2
        if code & 8:
            pred -= diffq
        else:
            pred += diffq
        if pred > 32767:
            pred = 32767
        elif pred < -32768:
            pred = -32768
        index += IMA_INDEX[code & 7]
        if index < 0:
            index = 0
        elif index > 88:
            index = 88

        if nibble_buf is None:
            nibble_buf = code & 0xF
        else:
            out.append((code & 0xF) << 4 | nibble_buf)
            nibble_buf = None
    if nibble_buf is not None:
        out.append(nibble_buf)
    return bytes(out)


def pcm_from_mp3(mp3: Path, pcm: Path) -> int:
    """Return sample count after silence trim + short pad (no speedup)."""
    af = (
        "silenceremove=start_periods=1:start_silence=0.015:start_threshold=-38dB:detection=peak,"
        "areverse,"
        "silenceremove=start_periods=1:start_silence=0.015:start_threshold=-38dB:detection=peak,"
        "areverse,"
        "apad=pad_dur=0.02"
    )
    subprocess.run(
        [
            "ffmpeg", "-y", "-i", str(mp3),
            "-af", af,
            "-ac", "1", "-ar", "16000", "-f", "s16le", str(pcm),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    # Hard-cap ~0.38s to control flash
    raw = pcm.read_bytes()
    max_bytes = int(16000 * 0.38 * 2)
    if len(raw) > max_bytes:
        raw = raw[:max_bytes]
        pcm.write_bytes(raw)
    return len(raw) // 2


async def tts_one(char: str, out_mp3: Path, voice: str, sem: asyncio.Semaphore) -> None:
    import edge_tts

    async with sem:
        for attempt in range(3):
            try:
                communicate = edge_tts.Communicate(char, voice=voice, rate="+0%")
                await communicate.save(str(out_mp3))
                if out_mp3.stat().st_size > 200:
                    return
            except Exception as exc:  # noqa: BLE001
                if attempt == 2:
                    raise RuntimeError(f"TTS failed for {char}: {exc}") from exc
                await asyncio.sleep(0.8 * (attempt + 1))


async def generate_all(limit: int | None, voice: str, concurrency: int) -> None:
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    chars = data["characters"]
    if limit is not None:
        chars = chars[:limit]

    WORK.mkdir(parents=True, exist_ok=True)
    sem = asyncio.Semaphore(concurrency)
    tasks = []
    for row in chars:
        mp3 = WORK / f"{row['index_global']:04d}.mp3"
        if mp3.exists() and mp3.stat().st_size > 200:
            continue
        tasks.append(tts_one(row["char"], mp3, voice, sem))
    if tasks:
        print(f"TTS generating {len(tasks)} clips with {voice}...")
        # chunk to avoid huge gather
        for i in range(0, len(tasks), 40):
            await asyncio.gather(*tasks[i : i + 40])
            print(f"  ... {min(i+40, len(tasks))}/{len(tasks)}")

    entries = []
    payloads = bytearray()
    failed = []
    for row in chars:
        idx = row["index_global"]
        mp3 = WORK / f"{idx:04d}.mp3"
        pcm = WORK / f"{idx:04d}.pcm"
        try:
            if not mp3.exists():
                raise FileNotFoundError(mp3)
            nsamp = pcm_from_mp3(mp3, pcm)
            samples = list(struct.unpack(f"<{nsamp}h", pcm.read_bytes()))
            adpcm = encode_ima_adpcm(samples)
        except Exception as exc:  # noqa: BLE001
            failed.append((idx, row["char"], str(exc)))
            continue
        offset = len(payloads)
        payloads += adpcm
        entries.append((idx, offset, len(adpcm), nsamp))

    # Pack binary: magic HZVP
    # header: magic(4) ver(u16) rate(u16) count(u16) reserved(u16)
    # entry: index(u16) reserved(u16) offset(u32) adpcm_len(u16) samples(u16)
    hdr = struct.pack("<4sHHHH", b"HZVP", 1, 16000, len(entries), 0)
    table = bytearray()
    data_base = 12 + len(entries) * 12
    for idx, offset, alen, nsamp in entries:
        table += struct.pack("<HH I HH", idx, 0, data_base + offset, alen, nsamp)

    OUT_BIN.parent.mkdir(parents=True, exist_ok=True)
    OUT_BIN.write_bytes(hdr + table + payloads)

    total = data["row_count"]
    covered = len(entries)
    pct = 100.0 * covered / total if total else 0
    books = {}
    for row in data["characters"]:
        b = row["book"]
        books.setdefault(b, {"total": 0, "have": 0})
        books[b]["total"] += 1
    have_set = {e[0] for e in entries}
    for row in data["characters"]:
        if row["index_global"] in have_set:
            books[row["book"]]["have"] += 1

    lines = [
        '<p align="right">',
        '  <a href="VOICE_COVERAGE.zh_CN.md">简体中文</a> · <strong>English</strong>',
        "</p>",
        "",
        "# Voice coverage",
        "",
        f"- Pack: `assets/voice/hanzi_voice_pack.bin` ({OUT_BIN.stat().st_size} bytes)",
        f"- Format: IMA-ADPCM @ 16 kHz mono (custom HZVP container)",
        f"- Coverage: **{covered}/{total} ({pct:.1f}%)** curriculum slots",
        f"- Voice engine: edge-tts `{voice}` rate=+0% (no atempo speedup)",
        f"- Regenerate: `python3 scripts/gen_voice_pack.py`",
        "",
        "| Book | Have | Total | % |",
        "| --- | ---: | ---: | ---: |",
    ]
    for b in sorted(books):
        h, t = books[b]["have"], books[b]["total"]
        lines.append(f"| {b} | {h} | {t} | {100*h/t:.0f}% |")
    if failed:
        lines += ["", "## Failures", ""]
        for idx, ch, err in failed[:20]:
            lines.append(f"- #{idx} {ch}: {err}")
    OUT_META.write_text("\n".join(lines) + "\n", encoding="utf-8")

    OUT_HDR.write_text(
        "\n".join(
            [
                "#pragma once",
                f"#define HANZI_VOICE_COUNT {covered}",
                f"#define HANZI_VOICE_CURRICULUM {total}",
                f"#define HANZI_VOICE_SAMPLE_RATE 16000",
                "",
            ]
        ),
        encoding="utf-8",
    )
    print(f"Wrote {OUT_BIN} ({OUT_BIN.stat().st_size} bytes), coverage {covered}/{total}")
    if failed:
        print(f"WARNING: {len(failed)} failures", file=sys.stderr)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=None, help="Only first N characters")
    ap.add_argument("--voice", default="zh-CN-XiaoxiaoNeural")
    ap.add_argument("--concurrency", type=int, default=6)
    ap.add_argument("--budget-bytes", type=int, default=0, help="If set, stop packing when data exceeds (after encoding)")
    args = ap.parse_args()
    asyncio.run(generate_all(args.limit, args.voice, args.concurrency))
    return 0


if __name__ == "__main__":
    sys.exit(main())
