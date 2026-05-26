from __future__ import annotations

import importlib
import importlib.util
from pathlib import Path
from types import ModuleType
from typing import Callable

from .model import SynthPatch


def _load_module(reference: str | Path) -> ModuleType:
    text = str(reference)
    path = Path(text)
    if path.exists():
        spec = importlib.util.spec_from_file_location(path.stem, path)
        if spec is None or spec.loader is None:
            raise ImportError(f"cannot load plugin module: {path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    return importlib.import_module(text)


def load_patch_plugin(reference: str | Path, factory: str = "create_patch", **kwargs: object) -> SynthPatch:
    """Load a Python patch generator.

    The plugin module must expose a factory function returning ``SynthPatch``.
    This is intentionally a patch-generation plugin layer, not native DSP hosting.
    """

    module = _load_module(reference)
    fn = getattr(module, factory, None)
    if fn is None or not callable(fn):
        raise AttributeError(f"plugin does not define callable {factory!r}")
    patch = fn(**kwargs)
    if not isinstance(patch, SynthPatch):
        raise TypeError(f"{factory} must return arachnotracker.SynthPatch")
    return patch
