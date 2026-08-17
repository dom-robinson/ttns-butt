#!/usr/bin/env python3
"""Analyze TTNS Deck monitor diagnostic WAVs (stdlib only).

Looks for evidence of:
  - upstream warble already in SplitCam/line capture
  - dropouts / silence holes
  - pitch wow (zero-crossing rate drift) introduced between line and monitor
  - L/R imbalance or decorrelation (channel bugs)
"""

from __future__ import annotations

import math
import struct
import sys
import wave
from pathlib import Path


def read_wav(path: Path):
    with wave.open(str(path), "rb") as w:
        ch = w.getnchannels()
        sw = w.getsampwidth()
        sr = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit(f"{path}: need 16-bit PCM, got sampwidth={sw}")
    samples = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    if ch == 1:
        left = list(samples)
        right = list(samples)
    else:
        left = list(samples[0::2])
        right = list(samples[1::2])
    return sr, left, right


def rms(xs):
    if not xs:
        return 0.0
    return math.sqrt(sum(x * x for x in xs) / len(xs))


def peak(xs):
    return max((abs(x) for x in xs), default=0)


def zero_cross_rate(xs):
    if len(xs) < 2:
        return 0.0
    zc = 0
    for i in range(1, len(xs)):
        if (xs[i - 1] >= 0) != (xs[i] >= 0):
            zc += 1
    return zc / (len(xs) - 1)


def window_stats(xs, sr, win_ms=50):
    win = max(8, int(sr * win_ms / 1000))
    hop = win // 2
    zcrs = []
    rmss = []
    for i in range(0, len(xs) - win, hop):
        chunk = xs[i : i + win]
        zcrs.append(zero_cross_rate(chunk))
        rmss.append(rms(chunk))
    return zcrs, rmss


def silence_holes(rmss, thresh_ratio=0.05):
    if not rmss:
        return 0, 0.0
    peak_r = max(rmss) or 1.0
    thr = peak_r * thresh_ratio
    holes = 0
    in_hole = False
    hole_frames = 0
    for r in rmss:
        if r < thr:
            hole_frames += 1
            if not in_hole:
                holes += 1
                in_hole = True
        else:
            in_hole = False
    return holes, hole_frames / len(rmss)


def corr(a, b):
    n = min(len(a), len(b))
    if n < 2:
        return 0.0
    a = a[:n]
    b = b[:n]
    ma = sum(a) / n
    mb = sum(b) / n
    num = sum((a[i] - ma) * (b[i] - mb) for i in range(n))
    da = math.sqrt(sum((a[i] - ma) ** 2 for i in range(n)))
    db = math.sqrt(sum((b[i] - mb) ** 2 for i in range(n)))
    if da < 1e-9 or db < 1e-9:
        return 0.0
    return num / (da * db)


def summarize(label, sr, left, right):
    zcrs, rmss = window_stats(left, sr)
    holes, hole_frac = silence_holes(rmss)
    z_mean = sum(zcrs) / len(zcrs) if zcrs else 0.0
    z_var = (
        sum((z - z_mean) ** 2 for z in zcrs) / len(zcrs) if zcrs else 0.0
    )
    z_std = math.sqrt(z_var)
    # Coefficient of variation of ZCR ~ proxy for pitch wow
    z_cv = (z_std / z_mean) if z_mean > 1e-9 else 0.0
    return {
        "label": label,
        "sr": sr,
        "seconds": len(left) / sr if sr else 0,
        "rms": rms(left),
        "peak": peak(left),
        "lr_corr": corr(left, right),
        "zcr_mean": z_mean,
        "zcr_cv": z_cv,
        "silence_holes": holes,
        "silence_frac": hole_frac,
        "rms_cv": (math.sqrt(sum((r - (sum(rmss) / len(rmss))) ** 2 for r in rmss) / len(rmss))
                   / (sum(rmss) / len(rmss)))
        if rmss and sum(rmss) > 0
        else 0.0,
    }


def print_summary(s):
    print(f"\n=== {s['label']} ===")
    print(f"  duration     {s['seconds']:.2f}s @ {s['sr']} Hz")
    print(f"  RMS / peak   {s['rms']:.1f} / {s['peak']}")
    print(f"  L/R corr     {s['lr_corr']:.4f}  (1.0=identical, <0.9 suspicious)")
    print(f"  ZCR CV       {s['zcr_cv']:.4f}  (pitch wow proxy; >0.08 suspicious)")
    print(f"  RMS CV       {s['rms_cv']:.4f}  (level pumping)")
    print(f"  silence holes {s['silence_holes']} ({100 * s['silence_frac']:.1f}% of windows)")


def main():
    log_dir = Path.home() / "Library/Logs/TTNS Deck"
    args = [Path(a) for a in sys.argv[1:]]
    if len(args) >= 2:
        line_p, mon_p = args[0], args[1]
        play_p = args[2] if len(args) >= 3 else log_dir / "diag-playback.wav"
    else:
        line_p = log_dir / "diag-line.wav"
        mon_p = log_dir / "diag-monitor.wav"
        play_p = log_dir / "diag-playback.wav"

    if not line_p.exists() or not mon_p.exists():
        print(f"Missing WAVs.\n  looked for:\n   {line_p}\n   {mon_p}")
        print("Play music through SplitCam with Deck monitor on for ~12s, then re-run.")
        return 1

    lsr, ll, lr = read_wav(line_p)
    msr, ml, mr = read_wav(mon_p)
    sl = summarize("LINE (SplitCam capture, pre-monitor)", lsr, ll, lr)
    sm = summarize("MONITOR QUEUE (written to ring before speakers)", msr, ml, mr)
    print_summary(sl)
    print_summary(sm)

    sp = None
    if play_p.exists():
        psr, pl, pr = read_wav(play_p)
        sp = summarize("PLAYBACK (exact PortAudio output to Speakers)", psr, pl, pr)
        print_summary(sp)

    print("\n=== VERDICT ===")
    if sl["rms"] < 50:
        print("Line capture is nearly silent — not feeding SplitCam / Music not routed.")
        return 0
    if sl["zcr_cv"] > 0.08 and sm["zcr_cv"] > 0.08 and abs(sl["zcr_cv"] - sm["zcr_cv"]) < 0.03:
        print("Warble/wow already present on LINE. Problem is UPSTREAM of Deck")
        print("(SplitCam / Music / aggregate), not sample-rate sync in our monitor path.")
    elif sm["zcr_cv"] > sl["zcr_cv"] + 0.04:
        print("Monitor queue wow is WORSE than line — introduced before playback")
        print("(mix/SRC/buffering).")
    elif sp and sp["zcr_cv"] > sm["zcr_cv"] + 0.04:
        print("Playback wow/worse than queue — PortAudio/CoreAudio output or underruns.")
    elif sp and (sp["silence_holes"] > sm["silence_holes"] + 3
                 or sp["silence_frac"] > sm["silence_frac"] + 0.05):
        print("Playback has extra silence holes vs queue — speaker underruns.")
    elif sm["silence_holes"] > sl["silence_holes"] + 3 or sm["silence_frac"] > sl["silence_frac"] + 0.05:
        print("Monitor queue has extra silence holes vs line — write drops / producer gaps.")
    elif sm["lr_corr"] < 0.85 and sl["lr_corr"] >= 0.85:
        print("L/R broke between line and monitor — stereo alignment / channel bug.")
    else:
        print("No strong wow/dropout signature difference in this capture.")
        print("If you still hear warble, listen to the three WAVs in Logs/TTNS Deck/")
        print("to localize by ear: diag-line vs diag-monitor vs diag-playback.")

    stats = log_dir / "diag-stats.txt"
    if stats.exists():
        print(f"\n=== {stats} ===")
        print(stats.read_text()[:4000])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
