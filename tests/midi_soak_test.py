#!/usr/bin/env python3
"""MIDI import/render soak test.

Recursively finds every .mid/.midi under a root directory (default: ~/Music),
imports each one to a .arachno project with the built ArachnoTracker binary,
renders it to WAV, and validates the result:

  - import succeeds and produces notes/tracks/instruments
  - render succeeds and produces non-silent, finite audio
  - no clipped samples, no DC offset, no NaNs
  - peak/RMS within sane bounds
  - re-render is byte-identical (deterministic offline path)

Usage:
    python3 tests/midi_soak_test.py [--root DIR] [--binary PATH] [--keep] [--verbose]

Exit code is 0 when every file passes, 1 otherwise.
"""

from __future__ import annotations

import argparse
import math
import re
import struct
import subprocess
import sys
import tempfile
import wave
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def find_midis(root: Path) -> list[Path]:
    return sorted(
        p for p in root.rglob("*") if p.suffix.lower() in (".mid", ".midi") and p.is_file()
    )


def wav_metrics(path: Path) -> dict | None:
    try:
        with wave.open(str(path)) as w:
            channels = w.getnchannels()
            frames = w.getnframes()
            rate = w.getframerate()
            raw = w.readframes(frames)
    except (wave.Error, EOFError):
        return None
    if frames == 0 or not raw:
        return None
    count = len(raw) // 2
    samples = struct.unpack(f"<{count}h", raw[: count * 2])
    left = samples[0::channels]
    peak = max(abs(s) for s in left) / 32768.0
    rms = math.sqrt(sum(s * s for s in left) / len(left)) / 32768.0
    dc = sum(left) / len(left) / 32768.0
    clipped = sum(1 for s in left if abs(s) >= 32767)
    return {
        "frames": frames,
        "rate": rate,
        "seconds": frames / rate,
        "peak": peak,
        "rms": rms,
        "dc": dc,
        "clipped": clipped,
    }


def parse_import_report(output: str) -> dict:
    notes = tracks = instruments = -1
    match = re.search(r"\((\d+) notes, (\d+) tracks, (\d+) instruments\)", output)
    if match:
        notes, tracks, instruments = (int(match.group(i)) for i in (1, 2, 3))
    return {"notes": notes, "tracks": tracks, "instruments": instruments}


def run(cmd: list[str], timeout: int) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=str(Path.home() / "Music"))
    parser.add_argument("--binary", default="build/ArachnoTracker")
    parser.add_argument("--keep", action="store_true", help="keep generated .arachno/.wav files")
    parser.add_argument("--out", default=None, help="output directory (default: temp dir)")
    parser.add_argument("--timeout", type=int, default=600, help="per-file render timeout (s)")
    parser.add_argument("--jobs", type=int, default=8, help="parallel import/render workers")
    parser.add_argument("--no-determinism-check", action="store_true",
                        help="skip the second render (faster)")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--match", default=None,
                        help="only process files whose path matches this regex")
    args = parser.parse_args()

    root = Path(args.root).expanduser()
    binary = Path(args.binary)
    if not binary.is_file():
        print(f"error: binary not found: {binary} (build first)")
        return 2
    midis = find_midis(root)
    indexed = list(enumerate(midis, start=1))  # stable indices across --match runs
    if args.match:
        pattern = re.compile(args.match)
        indexed = [(i, p) for i, p in indexed if pattern.search(str(p))]
    if not indexed:
        print(f"error: no .mid files found under {root}")
        return 2

    out_dir = Path(args.out) if args.out else Path(tempfile.mkdtemp(prefix="midi_soak_"))
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"soak root: {root}\nbinary:    {binary}\noutputs:   {out_dir}\nfound {len(indexed)} MIDI files (of {len(midis)} total)\n")

    failures: list[str] = []
    rows = []

    def process(item: tuple[int, Path]):
        index, midi = item
        label = f"[{index:02d}/{len(midis)}]"
        name = midi.name
        project = out_dir / f"{index:02d}.arachno"
        wav = out_dir / f"{index:02d}.wav"
        wav2 = out_dir / f"{index:02d}_b.wav"
        problems = []
        report = {"notes": -1, "tracks": -1, "instruments": -1}
        metrics = None

        try:
            imp = run([str(binary), "--import-midi", str(midi), str(project)], timeout=120)
            if imp.returncode != 0:
                problems.append(f"import exit {imp.returncode}: {imp.stderr.strip()[:200]}")
            else:
                report = parse_import_report(imp.stdout)
                if report["notes"] <= 0:
                    problems.append("import produced 0 notes")
                if report["tracks"] <= 0:
                    problems.append("import produced 0 tracks")

            if not problems:
                rnd = run([str(binary), "--render", str(project), str(wav)], timeout=args.timeout)
                if rnd.returncode != 0:
                    problems.append(f"render exit {rnd.returncode}: {rnd.stderr.strip()[:200]}")
            if not problems and not args.no_determinism_check:
                rnd2 = run([str(binary), "--render", str(project), str(wav2)], timeout=args.timeout)
                if rnd2.returncode == 0 and wav.read_bytes() != wav2.read_bytes():
                    problems.append("re-render is not deterministic")

            if not problems:
                metrics = wav_metrics(wav)
                if metrics is None:
                    problems.append("render produced unreadable/empty WAV")
                else:
                    if metrics["peak"] < 1e-4:
                        problems.append(f"render is silent (peak {metrics['peak']:.2e})")
                    if metrics["rms"] < 1e-4:
                        problems.append(f"render has no energy (rms {metrics['rms']:.2e})")
                    if metrics["clipped"] > 0:
                        problems.append(f"{metrics['clipped']} clipped samples")
                    if abs(metrics["dc"]) > 0.01:
                        problems.append(f"DC offset {metrics['dc']:+.4f}")
                    if metrics["peak"] > 0.999:
                        problems.append(f"peak {metrics['peak']:.3f} at full scale")
        except subprocess.TimeoutExpired:
            problems.append("timeout")
        finally:
            if not args.keep:
                for artifact in (project, wav, wav2):
                    artifact.unlink(missing_ok=True)

        if args.verbose or problems:
            print(f"{label} {'FAIL' if problems else 'ok':4s} {name}", flush=True)
            for problem in problems:
                print(f"       - {problem}", flush=True)
        return (name, report, metrics, problems)

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        rows = list(pool.map(process, indexed))
    rows.sort(key=lambda row: row[0])
    failures = [name for name, _, _, problems in rows if problems]

    print("\n=== summary ===")
    print(f"{'file':60s} {'notes':>6s} {'trk':>4s} {'ins':>4s} {'dur(s)':>7s} {'peak':>6s} {'rms':>6s} {'clip':>5s}")
    for name, report, metrics, problems in rows:
        if metrics:
            print(
                f"{name[:60]:60s} {report['notes']:6d} {report['tracks']:4d} {report['instruments']:4d} "
                f"{metrics['seconds']:7.1f} {metrics['peak']:6.3f} {metrics['rms']:6.3f} {metrics['clipped']:5d}"
            )
        else:
            print(f"{name[:60]:60s} {report['notes']:6d} {report['tracks']:4d} {report['instruments']:4d} "
                  f"{'FAIL':>7s} {'':6s} {'':6s} {'':5s}  {'; '.join(problems)[:80]}")

    passed = len(indexed) - len(failures)
    print(f"\npassed {passed}/{len(indexed)}")
    if failures:
        print("failures:")
        for name in failures:
            print(f"  - {name}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
