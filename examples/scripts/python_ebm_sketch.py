#!/usr/bin/env python3
from pathlib import Path

import arachnotracker as at


def build_song() -> at.Song:
    song = at.darkwave_ebm_starter("Python EBM Sketch", bpm=132)
    song.description = "Generated from examples/scripts/python_ebm_sketch.py"
    return song


if __name__ == "__main__":
    output = Path("python_ebm_sketch.arachno")
    at.save_project(build_song(), output)
    print(f"Wrote {output}")
