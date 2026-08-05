# Factory Patch Library

98 factory `.arachnopatch` presets, organized by category:

| Folder | Contents |
|--------|----------|
| `bass/` | EBM monoliths, reeses, acid, wobble, FM, and 808 basses |
| `drums/` | Kicks, snares, claps, hats, cymbals, toms, and percussion (incl. 808/909 classics) |
| `keys/` | Electric piano, organ, clav, bells, marimba, stabs |
| `lead/` | Saw/sync/square mono leads, rave hoovers, FM and acid leads |
| `pads/` | Warm analog, glass, hollow, soundtrack, choir, and drone pads |
| `strings/` | Classic synthesized strings: ensembles, string machines, pizzicato, tremolo, marcato |
| `arps/` | Pluck, acid, digital, rave, and sequencer arps |
| `fx/` | Risers, sweeps, impacts, sirens, static, and drops |
| `init.arachnopatch` | Default init patch (kept at the root) |

File numbering (`01_`, `02_`, ...) keeps bulk folder loading deterministic;
`01_*` files in each folder are the original C++ demo-song exports, renamed.

## Loading

- The GUI patch file browser navigates into category subfolders directly.
- Bulk "load folder" imports recurse through all subfolders, sorted by
  category-relative path.
- Saving a patch from the synth designer routes into the matching category
  folder automatically when the patch name maps to a known category.

## Regenerating

The entries after the `01_*` C++ exports are generated from the Python SDK:

```bash
PYTHONPATH=python python3 examples/scripts/generate_factory_patches.py
PYTHONPATH=python python3 examples/scripts/generate_factory_patches.py --check
```

Note: the Python SDK reads and writes the modern `arachno_patch 2` layout
(148 values) as well as every historical layout, so all entries — including
the `01_*` C++ exports and the `patches/gm/` bank — load in both the SDK and
the C++ application.
