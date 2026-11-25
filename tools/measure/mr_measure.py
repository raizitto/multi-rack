#!/usr/bin/env python3
"""
Multi Rack - black-box measurement kit.

Measures how an audio processor behaves (frequency response, saturation curve,
harmonics, compressor static curve and timing) by comparing a known test signal
with the processor's output. No access to the processor's code is needed.

Usage:
  python mr_measure.py generate [--rate 48000] [--out test_signal.wav]
  python mr_measure.py analyze  processed.wav [--dry test_signal.wav] [--out report]
  python mr_measure.py compare  reference.wav candidate.wav [--dry test_signal.wav] [--out compare]

Requirements: Python 3.9+, numpy, scipy, matplotlib  (pip install numpy scipy matplotlib)
"""
import argparse
import json
import os
import sys

import numpy as np
from scipy.io import wavfile
from scipy.signal import fftconvolve

# ----------------------------------------------------------------------------
# Test signal layout (seconds). Keep in sync between generate() and analyze().
# ----------------------------------------------------------------------------
GAP = 0.5
SWEEP_LEN = 10.0
SWEEP_LO, SWEEP_HI = 20.0, 20000.0
STEP_LEN = 0.4
STEP_LEVELS = list(range(-48, 7, 3))           # dBFS, -48 .. +6
TIMING_LEVELS = [(-30, 1.0), (-6, 1.5), (-30, 2.5), (-6, 1.5), (-30, 2.5)]  # (dBFS, seconds) at 1 kHz
FADE = 0.005


def segments(rate):
    """Returns ordered list of (name, start_sample, length_samples, info)."""
    segs = []
    t = GAP
    def add(name, length, info):
        nonlocal t
        segs.append((name, int(round(t * rate)), int(round(length * rate)), info))
        t += length + GAP
    add("sweep_low", SWEEP_LEN, {"level": -18.0})
    add("sweep_high", SWEEP_LEN, {"level": -6.0})
    add("steps_1k", STEP_LEN * len(STEP_LEVELS), {"freq": 1000.0})
    add("steps_100", STEP_LEN * len(STEP_LEVELS), {"freq": 100.0})
    add("steps_5k", STEP_LEN * len(STEP_LEVELS), {"freq": 5000.0})
    add("timing", sum(d for _, d in TIMING_LEVELS), {"freq": 1000.0})
    return segs, int(round(t * rate))


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def fade(sig, rate):
    n = int(FADE * rate)
    if n > 0 and len(sig) > 2 * n:
        w = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n))
        sig[:n] *= w
        sig[-n:] *= w[::-1]
    return sig


def make_sweep(rate, level_db):
    n = int(round(SWEEP_LEN * rate))
    t = np.arange(n) / rate
    k = np.log(SWEEP_HI / SWEEP_LO)
    phase = 2 * np.pi * SWEEP_LO * SWEEP_LEN / k * (np.exp(t * k / SWEEP_LEN) - 1)
    return fade(10 ** (level_db / 20) * np.sin(phase), rate)


def make_steps(rate, freq):
    out = []
    n = int(round(STEP_LEN * rate))
    t = np.arange(n) / rate
    for lv in STEP_LEVELS:
        out.append(fade(10 ** (lv / 20) * np.sin(2 * np.pi * freq * t), rate))
    return np.concatenate(out)


def make_timing(rate, freq=1000.0):
    total = int(round(sum(d for _, d in TIMING_LEVELS) * rate))
    t = np.arange(total) / rate
    amp = np.zeros(total)
    pos = 0
    for lv, d in TIMING_LEVELS:
        n = int(round(d * rate))
        amp[pos:pos + n] = 10 ** (lv / 20)
        pos += n
    return fade(amp * np.sin(2 * np.pi * freq * t), rate)


def generate(rate):
    segs, total = segments(rate)
    sig = np.zeros(total)
    for name, start, length, info in segs:
        if name.startswith("sweep"):
            s = make_sweep(rate, info["level"])
        elif name.startswith("steps"):
            s = make_steps(rate, info["freq"])
        else:
            s = make_timing(rate, info["freq"])
        sig[start:start + len(s)] = s[:length]
    return sig.astype(np.float32)


# ----------------------------------------------------------------------------
def read_wav(path):
    rate, data = wavfile.read(path)
    if data.dtype == np.int16:
        data = data.astype(np.float64) / 32768.0
    elif data.dtype == np.int32:
        data = data.astype(np.float64) / 2147483648.0
    elif data.dtype == np.uint8:
        data = (data.astype(np.float64) - 128) / 128.0
    else:
        data = data.astype(np.float64)
    if data.ndim == 2:
        data = data[:, 0]   # left channel
    return rate, data


def align(dry, wet, rate, max_lag_s=1.0):
    """Find the delay of wet relative to dry using the low-level sweep."""
    segs, _ = segments(rate)
    _, start, length, _ = segs[0]
    a = dry[start:start + length]
    lo = max(0, start - int(max_lag_s * rate))
    b = wet[lo:start + length + int(max_lag_s * rate)]
    corr = fftconvolve(b, a[::-1], mode="valid")
    lag = int(np.argmax(np.abs(corr))) + lo - start
    return lag


def tone_analysis(x, rate, freq, nharm=7):
    """Amplitudes (dBFS, peak) of the fundamental and harmonics of a steady tone."""
    n = len(x)
    w = np.blackman(n)
    spec = np.abs(np.fft.rfft(x * w)) * 2.0 / np.sum(w)
    bins = np.fft.rfftfreq(n, 1.0 / rate)
    res = []
    for h in range(1, nharm + 1):
        f = freq * h
        if f >= rate / 2 - 100:
            res.append(-200.0)
            continue
        i = int(round(f * n / rate))
        a = np.max(spec[max(0, i - 3):i + 4])
        res.append(float(db(a)))
    noise_floor = float(db(np.median(spec)))
    return res, noise_floor, bins


def smooth_response(freqs, mag_db, points):
    out = []
    for f in points:
        band = (freqs > f / 2 ** (1 / 12)) & (freqs < f * 2 ** (1 / 12))
        out.append(float(np.mean(mag_db[band])) if np.any(band) else float("nan"))
    return out


def analyze(dry, wet, rate):
    lag = align(dry, wet, rate)
    if lag > 0:
        wet = wet[lag:]
    elif lag < 0:
        wet = np.concatenate([np.zeros(-lag), wet])
    if len(wet) < len(dry):
        wet = np.concatenate([wet, np.zeros(len(dry) - len(wet))])
    wet = wet[:len(dry)]

    segs, _ = segments(rate)
    report = {"sample_rate": rate, "latency_samples": int(lag)}
    curves = {}
    points = [20, 30, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000,
              1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000]

    for name, start, length, info in segs:
        d = dry[start:start + length]
        w = wet[start:start + length]

        if name.startswith("sweep"):
            nfft = 1 << int(np.ceil(np.log2(length * 2)))
            D = np.fft.rfft(d, nfft)
            W = np.fft.rfft(w, nfft)
            freqs = np.fft.rfftfreq(nfft, 1.0 / rate)
            H = W * np.conj(D) / (np.abs(D) ** 2 + 1e-9 * np.max(np.abs(D)) ** 2)
            mag = db(H)
            resp = smooth_response(freqs, mag, points)
            ref1k = resp[points.index(1000)]
            fine_pts = [round(float(f), 1) for f in 20.0 * 2 ** (np.arange(0, 120) / 12.0) if f <= 18000]
            fine = smooth_response(freqs, mag, fine_pts)
            report[name] = {"level_dbfs": info["level"],
                            "gain_at_1k_db": round(ref1k, 2),
                            "response_db_rel_1k": {str(f): round(v - ref1k, 2) for f, v in zip(points, resp)},
                            "response_fine_db": {str(f): round(v, 2) for f, v in zip(fine_pts, fine)}}
            curves[name] = (points, [v - ref1k for v in resp], ref1k)

        elif name.startswith("steps"):
            n = int(round(STEP_LEN * rate))
            rows = []
            for k, lv in enumerate(STEP_LEVELS):
                a, b = start + k * n + int(0.1 * n), start + (k + 1) * n - int(0.1 * n)
                harm, floor, _ = tone_analysis(wet[a:b], rate, info["freq"])
                inp, _, _ = tone_analysis(dry[a:b], rate, info["freq"], 1)
                fund = harm[0]
                hs = [h - fund for h in harm[1:]]
                thd = float(np.sqrt(np.sum([10 ** (h / 10) for h in hs if h > -150])) * 100.0)
                rows.append({"in_dbfs": lv,
                             "out_dbfs": round(fund, 2),          # peak level of the fundamental
                             "gain_db": round(fund - inp[0], 2),
                             "thd_percent": round(thd, 4),
                             "harmonics_db_rel_fund": [round(h, 1) for h in hs]})
            report[name] = {"freq": info["freq"], "levels": rows}
            curves[name] = rows

        elif name == "timing":
            # gain envelope (dB) of wet / dry using 2 ms RMS windows
            win = max(1, int(0.002 * rate))
            def env(x):
                p = np.convolve(x ** 2, np.ones(win) / win, mode="same")
                return 10 * np.log10(np.maximum(p, 1e-14))
            g = env(w) - env(d)
            t = np.arange(len(g)) / rate
            events = []
            pos = 0.0
            for i, (lv, dur) in enumerate(TIMING_LEVELS):
                if i > 0:
                    events.append((pos, TIMING_LEVELS[i - 1][0], lv))
                pos += dur
            res = []
            for (te, frm, to) in events:
                i0 = int(te * rate)
                seg_end = min(len(g), i0 + int(1.4 * rate))
                pre = np.median(g[max(0, i0 - int(0.2 * rate)):i0 - int(0.01 * rate)])
                post = np.median(g[seg_end - int(0.2 * rate):seg_end])
                delta = post - pre
                tc = None
                if abs(delta) > 0.5:
                    target = pre + 0.632 * delta
                    seg = g[i0 + int(0.003 * rate):seg_end]
                    hit = np.where((seg - target) * np.sign(delta) >= 0)[0]
                    if len(hit):
                        # below ~3 ms the 2 ms analysis window dominates: report as "<3"
                        tc = "<3" if hit[0] == 0 else round(float(hit[0] / rate * 1000.0 + 3.0), 2)
                res.append({"at_s": round(te, 2), "step": f"{frm} -> {to} dBFS",
                            "gain_before_db": round(float(pre), 2), "gain_after_db": round(float(post), 2),
                            "time_constant_ms_63pct": tc,
                            "kind": "attack" if to > frm else "release"})
            report["timing"] = res
            curves["timing"] = (t, g)
    return report, curves


# ----------------------------------------------------------------------------
def plot(curves_list, labels, out_png):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(2, 3, figsize=(17, 9.5))
    fig.suptitle("Multi Rack measurement: " + " vs ".join(labels))

    for curves, label in zip(curves_list, labels):
        for name, axis in (("sweep_low", ax[0][0]), ("sweep_high", ax[0][1])):
            f, r, g = curves[name]
            axis.semilogx(f, r, marker=".", label=f"{label} (1k gain {g:+.2f} dB)")
        rows = curves["steps_1k"]
        ax[0][2].plot([r["in_dbfs"] for r in rows], [r["out_dbfs"] for r in rows], marker=".", label=label)
        for key, axis in (("steps_100", ax[1][0]), ("steps_1k", ax[1][1])):
            rows = curves[key]
            lv = [r["in_dbfs"] for r in rows]
            axis.plot(lv, [r["harmonics_db_rel_fund"][0] for r in rows], marker=".", label=f"{label} H2")
            axis.plot(lv, [r["harmonics_db_rel_fund"][1] for r in rows], marker="x", linestyle="--", label=f"{label} H3")
        t, g = curves["timing"]
        ax[1][2].plot(t, g, label=label)

    ax[0][0].set_title("Frequency response @ -18 dBFS (rel. 1 kHz)")
    ax[0][1].set_title("Frequency response @ -6 dBFS (rel. 1 kHz)")
    for a in (ax[0][0], ax[0][1]):
        a.set_xlabel("Hz"); a.set_ylabel("dB"); a.grid(True, which="both", alpha=0.3)
    ax[0][2].set_title("Transfer curve 1 kHz"); ax[0][2].set_xlabel("in dBFS"); ax[0][2].set_ylabel("out dBFS")
    ax[0][2].plot([-48, 6], [-48, 6], color="grey", linestyle=":", label="unity")
    ax[1][0].set_title("Harmonics 100 Hz (dB rel. fundamental)")
    ax[1][1].set_title("Harmonics 1 kHz (dB rel. fundamental)")
    for a in (ax[1][0], ax[1][1]):
        a.set_xlabel("in dBFS"); a.set_ylim(-140, 0)
    ax[1][2].set_title("Gain over time (level steps -30 / -6 dBFS)")
    ax[1][2].set_xlabel("s"); ax[1][2].set_ylabel("gain dB")
    for row in ax:
        for a in row:
            a.grid(True, alpha=0.3)
            a.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(out_png, dpi=110)


def print_summary(report, label=""):
    print(f"== {label}  (latency {report['latency_samples']} samples)")
    for k in ("sweep_low", "sweep_high"):
        r = report[k]
        resp = r["response_db_rel_1k"]
        print(f"  {k:10s} gain@1k {r['gain_at_1k_db']:+6.2f} dB | " +
              " ".join(f"{f}:{resp[f]:+.1f}" for f in ("30", "100", "300" if "300" in resp else "315", "3150", "10000", "16000")))
    for k in ("steps_100", "steps_1k", "steps_5k"):
        rows = report[k]["levels"]
        sel = [r for r in rows if r["in_dbfs"] in (-30, -18, -12, -6, 0, 6)]
        print(f"  {k:10s} " + " | ".join(
            f"{r['in_dbfs']:+d}: g{r['gain_db']:+.1f} thd{r['thd_percent']:.2f}% H2{r['harmonics_db_rel_fund'][0]:.0f} H3{r['harmonics_db_rel_fund'][1]:.0f}"
            for r in sel))
    for e in report["timing"]:
        print(f"  timing {e['step']:18s} {e['kind']:7s} gain {e['gain_before_db']:+.1f} -> {e['gain_after_db']:+.1f} dB, tau63 {e['time_constant_ms_63pct']} ms")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--rate", type=int, default=48000)
    g.add_argument("--out", default="test_signal.wav")
    a = sub.add_parser("analyze")
    a.add_argument("processed")
    a.add_argument("--dry", default=None, help="the unprocessed test signal (default: regenerated)")
    a.add_argument("--out", default=None, help="output prefix for .json/.png")
    c = sub.add_parser("compare")
    c.add_argument("reference")
    c.add_argument("candidate")
    c.add_argument("--dry", default=None)
    c.add_argument("--out", default="compare")
    args = ap.parse_args()

    if args.cmd == "generate":
        sig = generate(args.rate)
        wavfile.write(args.out, args.rate, sig)
        print(f"wrote {args.out}: {len(sig) / args.rate:.1f} s, {args.rate} Hz, 32-bit float mono")
        return

    def load_pair(path):
        rate, wet = read_wav(path)
        if args.dry:
            r2, dry = read_wav(args.dry)
            if r2 != rate:
                sys.exit(f"sample rate mismatch: {path} is {rate} Hz, dry is {r2} Hz")
        else:
            dry = generate(rate).astype(np.float64)
        return rate, dry, wet

    if args.cmd == "analyze":
        rate, dry, wet = load_pair(args.processed)
        report, curves = analyze(dry, wet, rate)
        prefix = args.out or os.path.splitext(args.processed)[0] + "_report"
        with open(prefix + ".json", "w") as f:
            json.dump(report, f, indent=1)
        plot([curves], [os.path.basename(args.processed)], prefix + ".png")
        print_summary(report, os.path.basename(args.processed))
        print(f"wrote {prefix}.json and {prefix}.png")
        return

    if args.cmd == "compare":
        out = []
        for p in (args.reference, args.candidate):
            rate, dry, wet = load_pair(p)
            out.append(analyze(dry, wet, rate) + (os.path.basename(p),))
        plot([o[1] for o in out], [o[2] for o in out], args.out + ".png")
        with open(args.out + ".json", "w") as f:
            json.dump({o[2]: o[0] for o in out}, f, indent=1)
        for rep, _, label in out:
            print_summary(rep, label)
        ra, rb = out[0][0], out[1][0]
        d_resp = [rb["sweep_low"]["response_db_rel_1k"][k] - ra["sweep_low"]["response_db_rel_1k"][k]
                  for k in ra["sweep_low"]["response_db_rel_1k"]]
        print(f"\nmax response difference @ -18 dBFS: {max(abs(x) for x in d_resp if x == x):.2f} dB")
        print(f"wrote {args.out}.json and {args.out}.png")


if __name__ == "__main__":
    main()
