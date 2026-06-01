# python/arachnotracker

## Purpose

This package is the Python SDK/API layer for generating and transforming ArachnoTracker assets.

Primary targets:

- `.arachno` project files
- `.arachnopatch` patch files

## Module Map

- `model.py`
  - Core Python data model mirroring tracker entities.

- `io.py`
  - Read/write for project and patch formats.

- `notes.py`
  - Note naming and pitch helpers.

- `presets.py`
  - Factory-like programmatic patch/project presets.

- `templates.py`
  - Template builders for common composition setups.

- `generative.py`
  - Pattern/song generation utilities for algorithmic composition.

- `plugins.py`
  - Plugin-style extension hooks.

- `commands.py`
  - SDK command helpers used by module CLI.

- `__main__.py`
  - `python -m arachnotracker` entrypoint.

## Usage

Run examples:

```bash
PYTHONPATH=python python3 examples/scripts/python_ebm_sketch.py
```

Module CLI:

```bash
PYTHONPATH=python python3 -m arachnotracker --help
```

## Compatibility Notes

- Keep format behavior synchronized with C++ project/patch parsers (`src/utils/ProjectIO.cpp`, `src/utils/PatchIO.cpp`).
- When adding SDK fields, verify round-trip load/save with both Python and C++ sides.

## Tests

```bash
PYTHONPATH=python python3 -m unittest discover -s python/tests
```

