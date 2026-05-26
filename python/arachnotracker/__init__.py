"""Public Python SDK for composing and transforming ArachnoTracker projects."""

from .commands import EditScript
from .io import load_patch, load_project, save_patch, save_project
from .model import (
    Envelope,
    Pattern,
    Song,
    Step,
    SynthPatch,
    Track,
    Waveform,
)
from .notes import midi_to_note_name, note_name_to_midi
from .plugins import load_patch_plugin
from .templates import darkwave_ebm_starter
from . import generative, presets

__all__ = [
    "EditScript",
    "Envelope",
    "Pattern",
    "Song",
    "Step",
    "SynthPatch",
    "Track",
    "Waveform",
    "load_patch_plugin",
    "load_patch",
    "load_project",
    "darkwave_ebm_starter",
    "generative",
    "midi_to_note_name",
    "note_name_to_midi",
    "presets",
    "save_patch",
    "save_project",
]
