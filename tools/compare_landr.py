"""Compare LANDR renders against Forge Master renders of the same mix.

LANDR files are matched by name: <anything>_<style>_<intensity>.wav, e.g.
Midnight_Balanced_Medium.wav. For each one, Forge Master renders the mix with
the same style/intensity and a loudness target equal to LANDR's measured
loudness, then both are measured side by side.

Usage: python compare_landr.py <mix.wav> <landr_dir> <out_dir>
"""
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
import pyloudnorm as pyln
import soundfile as sf
from scipy.signal import lfilter, resample_poly

HERE = Path(__file__).resolve().parent
RENDER = HERE.parent / "build/AutoMasterTest_artefacts/Release/AutoMasterTest"
STYLES = {"balanced": 0, "warm": 1, "open": 2}
INTENSITIES = {"low": 25, "medium": 50, "med": 50, "high": 80}
CENTRES = [40, 80, 160, 315, 630, 1250, 2500, 5000, 10000, 16000]


def octave_spectrum(mono, sr):
    """Same filterbank as MixAnalyzer, loudest sections only, aligned on 315 Hz-2.5 kHz."""
    blk = int(sr * 0.4)
    n = len(mono) // blk
    e = np.array([np.mean(mono[i * blk:(i + 1) * blk] ** 2) for i in range(n)])
    keep = np.repeat(10 * np.log10(e + 1e-12) > 10 * np.log10(np.percentile(e, 95)) - 8, blk)
    m = mono[:len(keep)]
    out = []
    for f in CENTRES:
        w = 2 * np.pi * min(f, 0.45 * sr) / sr
        al = np.sin(w) / (2 * 1.41)
        a0 = 1 + al
        y = lfilter([al / a0, 0, -al / a0], [1, -2 * np.cos(w) / a0, (1 - al) / a0], m)
        out.append(10 * np.log10(np.mean(y[keep] ** 2) + 1e-20))
    out = np.array(out)
    return out - out[3:7].mean()


def measure(path):
    x, sr = sf.read(path, always_2d=True)
    mid, side = x.mean(1), (x[:, 0] - x[:, 1]) / 2
    return {
        "lufs": pyln.Meter(sr).integrated_loudness(x),
        "tp": 20 * np.log10(np.max(np.abs(resample_poly(x, 4, 1, axis=0)))),
        "crest": 20 * np.log10(np.abs(x).max()) - 10 * np.log10(np.mean(x ** 2)),
        "side": 10 * np.log10(np.mean(side ** 2) / np.mean(mid ** 2)),
        "spec": octave_spectrum(mid, sr),
    }


def main(mix, landr_dir, out_dir):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    landr_files = sorted(Path(landr_dir).glob("*.wav"))
    pattern = re.compile(r"_(balanced|warm|open)_(low|med|medium|high)\.wav$", re.I)

    rows = []
    for lf in landr_files:
        m = pattern.search(lf.name)
        if not m:
            print(f"skip (name not <x>_<style>_<intensity>.wav): {lf.name}")
            continue
        style, intensity = m.group(1).lower(), m.group(2).lower()
        L = measure(lf)
        ff = out_dir / f"forge_{style}_{intensity}.wav"
        # target index 0 = Custom; the test host takes a target index, so pass Custom via the closest preset
        subprocess.run([str(RENDER), str(mix), str(ff), str(STYLES[style]), str(INTENSITIES[intensity]),
                        "8" if L["lufs"] > -10 else ("10" if L["lufs"] > -12.5 else "1"), "60"],
                       check=True, stdout=subprocess.DEVNULL)
        F = measure(ff)
        rows.append((f"{style}/{intensity}", L, F))

    for name, L, F in rows:
        print(f"\n=== {name}")
        print(f"{'':8s} {'LUFS':>7s} {'TP':>6s} {'crest':>6s} {'side':>6s}   " + " ".join(f"{c:>6d}" for c in CENTRES))
        for tag, M in (("LANDR", L), ("Forge", F)):
            print(f"{tag:8s} {M['lufs']:7.2f} {M['tp']:6.2f} {M['crest']:6.1f} {M['side']:6.1f}   "
                  + " ".join(f"{v:6.1f}" for v in M["spec"]))
        d = F["spec"] - L["spec"]
        print(f"{'diff':8s} {F['lufs'] - L['lufs']:7.2f} {F['tp'] - L['tp']:6.2f} {F['crest'] - L['crest']:6.1f} "
              f"{F['side'] - L['side']:6.1f}   " + " ".join(f"{v:6.1f}" for v in d))


if __name__ == "__main__":
    main(*sys.argv[1:4])
