from __future__ import annotations

import json
import shlex
from pathlib import Path
from typing import Callable, Iterable, Sequence

from .model import Envelope, Pattern, Song, Step, SynthPatch, Track, Waveform

PROJECT_VERSION = 1
PATCH_VERSION = 2

_Converter = Callable[[str], object]
_FieldSpec = tuple[str, _Converter]


def _as_float(value: str) -> float:
    return float(value)


def _as_int(value: str) -> int:
    return int(float(value))


def _as_waveform(value: str) -> Waveform:
    try:
        return Waveform(value)
    except ValueError as exc:
        raise ValueError(f"unknown waveform: {value}") from exc


# Waveform indices in C++ enum-declaration order (Sine=0 .. SuperSaw=5).
_WAVEFORMS_BY_INDEX: Sequence[Waveform] = tuple(Waveform)


def _waveform_to_index(waveform: Waveform) -> int:
    return _WAVEFORMS_BY_INDEX.index(waveform)


def _waveform_from_index(index: int) -> Waveform:
    # Mirrors the C++ reader: unknown indices fall back to Saw.
    if 0 <= index < len(_WAVEFORMS_BY_INDEX):
        return _WAVEFORMS_BY_INDEX[index]
    return Waveform.SAW


_PROJECT_PATCH_FIELDS: Sequence[_FieldSpec] = (
    ("oscillator_mix", _as_float),
    ("detune_cents", _as_float),
    ("sub_oscillator", _as_float),
    ("noise", _as_float),
    ("cutoff", _as_float),
    ("resonance", _as_float),
    ("filter_envelope_amount", _as_float),
    ("lfo_rate", _as_float),
    ("vibrato_cents", _as_float),
    ("tremolo_depth", _as_float),
    ("drive", _as_float),
    ("gain", _as_float),
    ("pan", _as_float),
    ("amp_envelope.attack", _as_float),
    ("amp_envelope.decay", _as_float),
    ("amp_envelope.sustain", _as_float),
    ("amp_envelope.release", _as_float),
    ("filter_envelope.attack", _as_float),
    ("filter_envelope.decay", _as_float),
    ("filter_envelope.sustain", _as_float),
    ("filter_envelope.release", _as_float),
    ("pitch_envelope_semitones", _as_float),
    ("pitch_envelope_decay", _as_float),
    ("ring_mod", _as_float),
    ("hard_sync", _as_float),
    ("bit_crush", _as_float),
    ("sample_rate_reduction", _as_float),
    ("high_pass", _as_float),
    ("unison_voices", _as_int),
    ("unison_detune_cents", _as_float),
    ("stereo_spread", _as_float),
    ("click", _as_float),
    ("transient_noise", _as_float),
    ("transient_decay", _as_float),
    ("pulse_width", _as_float),
    ("pwm_depth", _as_float),
    ("fm_amount", _as_float),
    ("fm_ratio", _as_float),
    ("chorus_mix", _as_float),
    ("chorus_rate", _as_float),
    ("chorus_depth", _as_float),
)

_PATCH_FILE_FIELDS: Sequence[_FieldSpec] = (
    ("oscillator_mix", _as_float),
    ("detune_cents", _as_float),
    ("pulse_width", _as_float),
    ("pwm_depth", _as_float),
    ("fm_amount", _as_float),
    ("fm_ratio", _as_float),
    ("chorus_mix", _as_float),
    ("chorus_rate", _as_float),
    ("chorus_depth", _as_float),
    ("unison_voices", _as_int),
    ("unison_detune_cents", _as_float),
    ("stereo_spread", _as_float),
    ("sub_oscillator", _as_float),
    ("noise", _as_float),
    ("cutoff", _as_float),
    ("resonance", _as_float),
    ("filter_envelope_amount", _as_float),
    ("pitch_envelope_semitones", _as_float),
    ("pitch_envelope_decay", _as_float),
    ("lfo_rate", _as_float),
    ("vibrato_cents", _as_float),
    ("tremolo_depth", _as_float),
    ("ring_mod", _as_float),
    ("hard_sync", _as_float),
    ("drive", _as_float),
    ("bit_crush", _as_float),
    ("sample_rate_reduction", _as_float),
    ("high_pass", _as_float),
    ("click", _as_float),
    ("transient_noise", _as_float),
    ("transient_decay", _as_float),
    ("gain", _as_float),
    ("pan", _as_float),
)


def _quoted(value: str) -> str:
    return json.dumps(value)


def _bool(value: bool) -> str:
    return "1" if value else "0"


def _envelope_values(envelope: Envelope) -> Iterable[float]:
    return (envelope.attack, envelope.decay, envelope.sustain, envelope.release)


def _patch_project_values(patch: SynthPatch) -> Iterable[object]:
    """Emit the full C++ ProjectIO instrument-line chain (161 values).

    The first 42 values match the historical SDK layout exactly; the remaining
    values mirror the C++ writer field-for-field so SDK projects can carry the
    extended engine surface (ensemble chorus, delay/reverb, portamento, layered
    oscillator ratios/decays) and load identically in the native app. The
    patch-layout velocity block (patch-file indices 138-145) is deliberately
    omitted: C++ ProjectIO does not carry it.
    """

    return (
        patch.oscillator_mix,  # 1
        patch.detune_cents,
        patch.sub_oscillator,
        patch.noise,
        patch.cutoff,  # 5
        patch.resonance,
        patch.filter_envelope_amount,
        patch.lfo_rate,
        patch.vibrato_cents,
        patch.tremolo_depth,  # 10
        patch.drive,
        patch.gain,
        patch.pan,
        *_envelope_values(patch.amp_envelope),  # 14-17
        *_envelope_values(patch.filter_envelope),  # 18-21
        patch.pitch_envelope_semitones,
        patch.pitch_envelope_decay,
        patch.ring_mod,
        patch.hard_sync,  # 25
        patch.bit_crush,
        patch.sample_rate_reduction,
        patch.high_pass,
        patch.unison_voices,
        patch.unison_detune_cents,  # 30
        patch.stereo_spread,
        patch.click,
        patch.transient_noise,
        patch.transient_decay,
        patch.pulse_width,  # 35
        patch.pwm_depth,
        patch.fm_amount,
        patch.fm_ratio,
        patch.chorus_mix,
        patch.chorus_rate,  # 40
        patch.chorus_depth,
        _waveform_to_index(patch.oscillator_c),  # 42 oscillatorC index
        1.0,  # 43 oscillatorAEnabled
        1.0 if patch.oscillator_b_enabled else 0.0,  # 44 oscillatorBEnabled
        1.0 if patch.oscillator_c_enabled else 0.0,  # 45 oscillatorCEnabled
        patch.oscillator_c_mix,  # 46 oscillatorCMix
        patch.detune_c_cents,  # 47 detuneCCents
        1.0 if patch.sub_oscillator > 0.0 else 0.0,  # 48 subEnabled
        1.0 if patch.noise > 0.0 else 0.0,  # 49 noiseEnabled
        1.0 if patch.fm_amount > 0.0 else 0.0,  # 50 fmEnabled
        patch.fm_feedback,  # 51 fmFeedback
        1.0 if patch.ring_mod > 0.0 else 0.0,  # 52 ringEnabled
        1.0 if patch.hard_sync > 0.0 else 0.0,  # 53 hardSyncEnabled
        1.0 if (patch.chorus_mix > 0.0 or patch.chorus_ensemble > 0.0) else 0.0,  # 54 chorusEnabled
        1.0 if (patch.bit_crush > 0.0 or patch.sample_rate_reduction > 0.0) else 0.0,  # 55 bitCrushEnabled
        patch.wavefold,  # 56 wavefold
        patch.comb_mix,  # 57 combMix
        patch.comb_time,  # 58 combTime
        patch.comb_feedback,  # 59 combFeedback
        patch.lfo_filter_depth,  # 60 lfoFilterDepth
        patch.lfo_pan_depth,  # 61 lfoPanDepth
        _waveform_to_index(patch.oscillator_d),  # 62 oscillatorD index
        1.0 if patch.oscillator_d_enabled else 0.0,  # 63 oscillatorDEnabled
        patch.oscillator_d_mix,  # 64 oscillatorDMix
        patch.detune_d_cents,  # 65 detuneDCents
        patch.noise_tone,  # 66
        patch.transient_shape,
        patch.transient_pitch_semitones,
        patch.transient_pitch_decay,
        patch.transient_burst_count,  # 70
        patch.transient_burst_spacing,
        patch.transient_burst_decay,
        patch.transient_tone,
        patch.fm_algorithm,  # 74 fmAlgorithm
        patch.filter_mode,  # 75 filterMode
        patch.filter_drive,  # 76
        patch.filter_keytrack,
        patch.analog_color,  # 78 analogColor
        patch.tone_tilt,  # 79 toneTilt
        patch.osc_a_level, patch.osc_b_level, patch.osc_c_level, patch.osc_d_level,  # 80-83
        patch.osc_a_detune_cents, patch.osc_b_detune_cents,  # 84-85
        patch.osc_c_detune_cents, patch.osc_d_detune_cents,  # 86-87
        patch.osc_a_pulse_width, patch.osc_b_pulse_width,  # 88-89
        patch.osc_c_pulse_width, patch.osc_d_pulse_width,  # 90-91
        patch.osc_a_pwm_depth, patch.osc_b_pwm_depth,  # 92-93
        patch.osc_c_pwm_depth, patch.osc_d_pwm_depth,  # 94-95
        patch.osc_a_drive, patch.osc_b_drive, patch.osc_c_drive, patch.osc_d_drive,  # 96-99
        patch.delay_mix,  # 100
        patch.delay_time,
        patch.delay_feedback,
        patch.delay_tone,  # 103 delayTone
        patch.reverb_mix,  # 104
        patch.reverb_size,
        patch.reverb_damping,
        patch.reverb_pre_delay,  # 107 reverbPreDelay
        patch.vintage_drift,  # 108 vintageDrift
        patch.wow_flutter,  # 109 wowFlutter
        patch.chorus_feedback,  # 110 chorusFeedback
        patch.chorus_delay,  # 111 chorusDelay
        patch.chorus_width,  # 112 chorusWidth
        patch.chorus_ensemble,  # 113
        patch.delay_stereo,  # 114 delayStereo
        patch.delay_mod_depth,  # 115 delayModDepth
        patch.delay_drive,  # 116 delayDrive
        patch.delay_ducking,  # 117 delayDucking
        patch.reverb_diffusion,  # 118 reverbDiffusion
        patch.reverb_width,  # 119 reverbWidth
        patch.reverb_shimmer,  # 120 reverbShimmer
        patch.reverb_mod_depth,  # 121 reverbModDepth
        patch.tape_color,  # 122 tapeColor
        patch.air_boost,  # 123 airBoost
        patch.low_punch,  # 124 lowPunch
        patch.analog_warmth,  # 125 analogWarmth
        patch.voice_slop,  # 126 voiceSlop
        patch.phase_scatter,  # 127 phaseScatter
        patch.chorus_tone,  # 128 chorusTone
        patch.delay_diffusion,  # 129 delayDiffusion
        patch.reverb_decay,  # 130
        patch.reverb_early_mix,  # 131 reverbEarlyMix
        patch.console_crosstalk,  # 132 consoleCrosstalk
        patch.output_glue,  # 133 outputGlue
        patch.unison_warp,  # 134 unisonWarp
        patch.unison_humanize,  # 135 unisonHumanize
        patch.fm_color,  # 136 fmColor
        patch.fm_spread,  # 137 fmSpread
        patch.chorus_jitter,  # 138 chorusJitter
        patch.chorus_saturation,  # 139 chorusSaturation
        patch.delay_wow,  # 140 delayWow
        patch.delay_crossfeed,  # 141 delayCrossfeed
        patch.reverb_tone,  # 142 reverbTone
        patch.reverb_chorus,  # 143 reverbChorus
        patch.reverb_bloom,  # 144 reverbBloom
        patch.stereo_depth,  # 145 stereoDepth
        patch.hifi_exciter,  # 146 hifiExciter
        patch.output_transformer,  # 147 outputTransformer
        patch.output_soft_clip,  # 148 outputSoftClip
        patch.portamento_time,  # 149
        1.0 if patch.portamento_legato else 0.0,  # 150
        patch.fm_decay,  # 151
        patch.velocity_to_fm,  # 152
        1.0 if patch.mono_mode else 0.0,  # 153
        patch.osc_b_ratio,  # 154
        patch.osc_c_ratio,  # 155
        patch.osc_d_ratio,  # 156
        patch.osc_b_decay,  # 157
        patch.osc_c_decay,  # 158
        patch.osc_d_decay,  # 159
        patch.velocity_to_decay,  # 160
        patch.key_track_decay,  # 161
    )


def _patch_file_values(patch: SynthPatch) -> Iterable[object]:
    """Emit the full modern 159-value ``params`` layout written by C++ PatchIO v2.

    Every field maps to the SDK model (defaults mirror the C++ ``SynthPatch``),
    so patches round-trip through the native app unchanged.
    """

    return (
        1.0,  # 0 oscillatorAEnabled
        1.0 if patch.oscillator_b_enabled else 0.0,  # 1 oscillatorBEnabled
        1.0 if patch.oscillator_c_enabled else 0.0,  # 2 oscillatorCEnabled
        1.0 if patch.oscillator_d_enabled else 0.0,  # 3 oscillatorDEnabled
        patch.oscillator_mix,  # 4
        patch.oscillator_c_mix,  # 5 oscillatorCMix
        patch.oscillator_d_mix,  # 6 oscillatorDMix
        patch.detune_cents,  # 7
        patch.detune_c_cents,  # 8 detuneCCents
        patch.detune_d_cents,  # 9 detuneDCents
        patch.pulse_width,  # 10
        patch.pwm_depth,  # 11
        1.0 if patch.fm_amount > 0.0 else 0.0,  # 12 fmEnabled
        patch.fm_amount,  # 13
        patch.fm_ratio,  # 14
        patch.fm_feedback,  # 15 fmFeedback
        patch.fm_algorithm,  # 16 fmAlgorithm
        1.0 if (patch.chorus_mix > 0.0 or patch.chorus_ensemble > 0.0) else 0.0,  # 17 chorusEnabled
        patch.chorus_mix,  # 18
        patch.chorus_rate,  # 19
        patch.chorus_depth,  # 20
        patch.chorus_feedback,  # 21 chorusFeedback
        patch.chorus_delay,  # 22 chorusDelay
        patch.chorus_width,  # 23 chorusWidth
        patch.unison_voices,  # 24
        patch.unison_detune_cents,  # 25
        patch.stereo_spread,  # 26
        1.0 if patch.sub_oscillator > 0.0 else 0.0,  # 27 subEnabled
        patch.sub_oscillator,  # 28
        1.0 if patch.noise > 0.0 else 0.0,  # 29 noiseEnabled
        patch.noise,  # 30
        patch.noise_tone,  # 31
        patch.cutoff,  # 32
        patch.resonance,  # 33
        patch.filter_mode,  # 34 filterMode
        patch.filter_drive,  # 35
        patch.filter_keytrack,  # 36
        patch.filter_envelope_amount,  # 37
        patch.lfo_filter_depth,  # 38 lfoFilterDepth
        patch.lfo_pan_depth,  # 39 lfoPanDepth
        patch.pitch_envelope_semitones,  # 40
        patch.pitch_envelope_decay,  # 41
        patch.lfo_rate,  # 42
        patch.vibrato_cents,  # 43
        patch.tremolo_depth,  # 44
        1.0 if patch.ring_mod > 0.0 else 0.0,  # 45 ringEnabled
        patch.ring_mod,  # 46
        1.0 if patch.hard_sync > 0.0 else 0.0,  # 47 hardSyncEnabled
        patch.hard_sync,  # 48
        patch.drive,  # 49
        patch.wavefold,  # 50 wavefold
        1.0 if (patch.bit_crush > 0.0 or patch.sample_rate_reduction > 0.0) else 0.0,  # 51 bitCrushEnabled
        patch.bit_crush,  # 52
        patch.sample_rate_reduction,  # 53
        patch.comb_mix,  # 54 combMix
        patch.comb_time,  # 55 combTime
        patch.comb_feedback,  # 56 combFeedback
        patch.delay_mix,  # 57
        patch.delay_time,  # 58
        patch.delay_feedback,  # 59
        patch.delay_tone,  # 60 delayTone
        patch.reverb_mix,  # 61
        patch.reverb_size,  # 62
        patch.reverb_damping,  # 63
        patch.reverb_pre_delay,  # 64 reverbPreDelay
        patch.high_pass,  # 65
        patch.click,  # 66
        patch.transient_shape,  # 67
        patch.transient_noise,  # 68
        patch.transient_pitch_semitones,  # 69
        patch.transient_pitch_decay,  # 70
        patch.transient_burst_count,  # 71
        patch.transient_burst_spacing,  # 72
        patch.transient_burst_decay,  # 73
        patch.transient_tone,  # 74
        patch.transient_decay,  # 75
        patch.analog_color,  # 76 analogColor
        patch.vintage_drift,  # 77 vintageDrift
        patch.wow_flutter,  # 78 wowFlutter
        patch.tone_tilt,  # 79 toneTilt
        patch.gain,  # 80
        patch.pan,  # 81
        patch.osc_a_level, patch.osc_b_level, patch.osc_c_level, patch.osc_d_level,  # 82-85
        patch.osc_a_detune_cents, patch.osc_b_detune_cents,  # 86-87
        patch.osc_c_detune_cents, patch.osc_d_detune_cents,  # 88-89
        patch.osc_a_pulse_width, patch.osc_b_pulse_width,  # 90-91
        patch.osc_c_pulse_width, patch.osc_d_pulse_width,  # 92-93
        patch.osc_a_pwm_depth, patch.osc_b_pwm_depth,  # 94-95
        patch.osc_c_pwm_depth, patch.osc_d_pwm_depth,  # 96-97
        patch.osc_a_drive, patch.osc_b_drive, patch.osc_c_drive, patch.osc_d_drive,  # 98-101
        patch.chorus_ensemble,  # 102
        patch.delay_stereo,  # 103 delayStereo
        patch.delay_mod_depth,  # 104 delayModDepth
        patch.delay_drive,  # 105 delayDrive
        patch.delay_ducking,  # 106 delayDucking
        patch.reverb_diffusion,  # 107 reverbDiffusion
        patch.reverb_width,  # 108 reverbWidth
        patch.reverb_shimmer,  # 109 reverbShimmer
        patch.reverb_mod_depth,  # 110 reverbModDepth
        patch.tape_color,  # 111 tapeColor
        patch.air_boost,  # 112 airBoost
        patch.low_punch,  # 113 lowPunch
        patch.analog_warmth,  # 114 analogWarmth
        patch.voice_slop,  # 115 voiceSlop
        patch.phase_scatter,  # 116 phaseScatter
        patch.chorus_tone,  # 117 chorusTone
        patch.delay_diffusion,  # 118 delayDiffusion
        patch.reverb_decay,  # 119
        patch.reverb_early_mix,  # 120 reverbEarlyMix
        patch.console_crosstalk,  # 121 consoleCrosstalk
        patch.output_glue,  # 122 outputGlue
        patch.unison_warp,  # 123 unisonWarp
        patch.unison_humanize,  # 124 unisonHumanize
        patch.fm_color,  # 125 fmColor
        patch.fm_spread,  # 126 fmSpread
        patch.chorus_jitter,  # 127 chorusJitter
        patch.chorus_saturation,  # 128 chorusSaturation
        patch.delay_wow,  # 129 delayWow
        patch.delay_crossfeed,  # 130 delayCrossfeed
        patch.reverb_tone,  # 131 reverbTone
        patch.reverb_chorus,  # 132 reverbChorus
        patch.reverb_bloom,  # 133 reverbBloom
        patch.stereo_depth,  # 134 stereoDepth
        patch.hifi_exciter,  # 135 hifiExciter
        patch.output_transformer,  # 136 outputTransformer
        patch.output_soft_clip,  # 137 outputSoftClip
        patch.velocity_to_amp,  # 138 velocityToAmp
        patch.velocity_to_filter,  # 139 velocityToFilter
        patch.velocity_to_attack,  # 140 velocityToAttack
        patch.velocity_curve,  # 141 velocityCurve
        patch.filter_keytrack_resonance,  # 142 filterKeytrackResonance
        patch.filter_nonlinearity,  # 143 filterNonlinearity
        patch.amp_envelope_curve,  # 144 ampEnvelopeCurve
        patch.filter_envelope_curve,  # 145 filterEnvelopeCurve
        patch.portamento_time,  # 146
        1.0 if patch.portamento_legato else 0.0,  # 147
        patch.fm_decay,  # 148
        patch.velocity_to_fm,  # 149
        1.0 if patch.mono_mode else 0.0,  # 150
        patch.osc_b_ratio,  # 151
        patch.osc_c_ratio,  # 152
        patch.osc_d_ratio,  # 153
        patch.osc_b_decay,  # 154
        patch.osc_c_decay,  # 155
        patch.osc_d_decay,  # 156
        patch.velocity_to_decay,  # 157
        patch.key_track_decay,  # 158
    )


def _format_values(values: Iterable[object]) -> str:
    return " ".join(str(value) for value in values)


def _split(line: str) -> list[str]:
    return shlex.split(line, posix=True)


def _expect(tokens: Sequence[str], keyword: str, path: Path, line_number: int) -> None:
    if not tokens or tokens[0] != keyword:
        actual = tokens[0] if tokens else "<empty>"
        raise ValueError(f"{path}:{line_number}: expected {keyword!r}, got {actual!r}")


def _set_path(target: object, path: str, value: object) -> None:
    if "." not in path:
        setattr(target, path, value)
        return
    parent_name, child_name = path.split(".", 1)
    setattr(getattr(target, parent_name), child_name, value)


def _apply_fields(patch: SynthPatch, fields: Sequence[_FieldSpec], values: Sequence[str]) -> None:
    for (name, converter), raw in zip(fields, values):
        _set_path(patch, name, converter(raw))


def _read_lines(path: str | Path) -> tuple[Path, list[tuple[int, list[str]]]]:
    resolved = Path(path)
    lines: list[tuple[int, list[str]]] = []
    for line_number, raw in enumerate(resolved.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = raw.strip()
        if stripped:
            lines.append((line_number, _split(stripped)))
    return resolved, lines


def _parse_envelope(tokens: Sequence[str], *, start: int = 1) -> Envelope:
    if len(tokens) < start + 4:
        raise ValueError("envelope line must contain attack, decay, sustain, and release")
    return Envelope(
        attack=float(tokens[start]),
        decay=float(tokens[start + 1]),
        sustain=float(tokens[start + 2]),
        release=float(tokens[start + 3]),
    )


def _serialize_step(row: int, track: int, step: Step) -> str:
    parts = ["step", row, track, 1 if step.midi is not None else 0]
    if step.midi is not None:
        parts.extend([step.midi, step.velocity])
    parts.extend([
        step.instrument,
        step.gate,
        step.micro_offset_rows,
        len(step.automation),
    ])
    for name, value in sorted(step.automation.items()):
        parts.extend([_quoted(name), value])
    parts.extend([
        1 if step.probability is not None else 0,
        step.probability if step.probability is not None else 1.0,
        step.retrigger_count,
        step.retrigger_spacing_rows,
        step.retrigger_velocity_decay,
        1 if step.note_off else 0,
    ])
    return _format_values(parts)


def validate_song(song: Song) -> None:
    if song.bpm <= 0:
        raise ValueError("song bpm must be positive")
    if song.rows_per_beat <= 0:
        raise ValueError("rows_per_beat must be positive")
    if song.sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if not song.tracks:
        raise ValueError("song must have at least one track")
    if not song.instruments:
        raise ValueError("song must have at least one instrument")
    if not song.patterns:
        raise ValueError("song must have at least one pattern")
    for index in song.order:
        if index < 0 or index >= len(song.patterns):
            raise ValueError(f"order contains invalid pattern index: {index}")


def load_project(path: str | Path) -> Song:
    """Load a native ``.arachno`` project into editable Python objects."""

    resolved, lines = _read_lines(path)
    cursor = 0

    def take(keyword: str) -> tuple[int, list[str]]:
        nonlocal cursor
        if cursor >= len(lines):
            raise ValueError(f"{resolved}: expected {keyword!r}, reached end of file")
        line_number, tokens = lines[cursor]
        cursor += 1
        _expect(tokens, keyword, resolved, line_number)
        return line_number, tokens

    _, header = take("arachno_project")
    if len(header) < 2 or int(header[1]) > PROJECT_VERSION:
        raise ValueError(f"{resolved}: unsupported project version")

    song = Song()
    song.title = take("title")[1][1]
    song.author = take("author")[1][1]
    song.description = take("description")[1][1]
    song.notes = take("notes")[1][1]
    song.bpm = float(take("bpm")[1][1])
    song.rows_per_beat = int(take("rows_per_beat")[1][1])
    song.sample_rate = int(take("sample_rate")[1][1])

    _, track_header = take("tracks")
    for _ in range(int(track_header[1])):
        line_number, tokens = take("track")
        if len(tokens) < 6:
            raise ValueError(f"{resolved}:{line_number}: track line is incomplete")
        song.tracks.append(
            Track(
                name=tokens[1],
                volume=float(tokens[2]),
                pan=float(tokens[3]),
                muted=bool(int(tokens[4])),
                solo=bool(int(tokens[5])),
            )
        )

    _, instrument_header = take("instruments")
    for expected_index in range(int(instrument_header[1])):
        line_number, tokens = take("instrument")
        if len(tokens) < 5:
            raise ValueError(f"{resolved}:{line_number}: instrument line is incomplete")
        index = int(tokens[1])
        if index != expected_index:
            raise ValueError(f"{resolved}:{line_number}: expected instrument {expected_index}, got {index}")
        patch = SynthPatch(
            name=tokens[2],
            oscillator_a=_as_waveform(tokens[3]),
            oscillator_b=_as_waveform(tokens[4]),
        )
        _apply_fields(patch, _PROJECT_PATCH_FIELDS, tokens[5:])
        # Extended fields from the full C++ instrument-line chain (positions are
        # stable across versions because the chain only ever grew by appending).
        extra = tokens[5:]

        def _extended(index: int) -> float | None:
            return float(extra[index]) if len(extra) > index else None

        for index, attr in (
            (45, "oscillator_c_mix"),
            (46, "detune_c_cents"),
            (50, "fm_feedback"),
            (55, "wavefold"),
            (56, "comb_mix"),
            (57, "comb_time"),
            (58, "comb_feedback"),
            (59, "lfo_filter_depth"),
            (60, "lfo_pan_depth"),
            (63, "oscillator_d_mix"),
            (64, "detune_d_cents"),
            (75, "filter_drive"),
            (76, "filter_keytrack"),
            (77, "analog_color"),
            (78, "tone_tilt"),
            (79, "osc_a_level"),
            (80, "osc_b_level"),
            (81, "osc_c_level"),
            (82, "osc_d_level"),
            (83, "osc_a_detune_cents"),
            (84, "osc_b_detune_cents"),
            (85, "osc_c_detune_cents"),
            (86, "osc_d_detune_cents"),
            (87, "osc_a_pulse_width"),
            (88, "osc_b_pulse_width"),
            (89, "osc_c_pulse_width"),
            (90, "osc_d_pulse_width"),
            (91, "osc_a_pwm_depth"),
            (92, "osc_b_pwm_depth"),
            (93, "osc_c_pwm_depth"),
            (94, "osc_d_pwm_depth"),
            (95, "osc_a_drive"),
            (96, "osc_b_drive"),
            (97, "osc_c_drive"),
            (98, "osc_d_drive"),
            (99, "delay_mix"),
            (100, "delay_time"),
            (101, "delay_feedback"),
            (102, "delay_tone"),
            (103, "reverb_mix"),
            (104, "reverb_size"),
            (105, "reverb_damping"),
            (106, "reverb_pre_delay"),
            (107, "vintage_drift"),
            (108, "wow_flutter"),
            (109, "chorus_feedback"),
            (110, "chorus_delay"),
            (111, "chorus_width"),
            (112, "chorus_ensemble"),
            (113, "delay_stereo"),
            (114, "delay_mod_depth"),
            (115, "delay_drive"),
            (116, "delay_ducking"),
            (117, "reverb_diffusion"),
            (118, "reverb_width"),
            (119, "reverb_shimmer"),
            (120, "reverb_mod_depth"),
            (121, "tape_color"),
            (122, "air_boost"),
            (123, "low_punch"),
            (124, "analog_warmth"),
            (125, "voice_slop"),
            (126, "phase_scatter"),
            (127, "chorus_tone"),
            (128, "delay_diffusion"),
            (129, "reverb_decay"),
            (130, "reverb_early_mix"),
            (131, "console_crosstalk"),
            (132, "output_glue"),
            (133, "unison_warp"),
            (134, "unison_humanize"),
            (135, "fm_color"),
            (136, "fm_spread"),
            (137, "chorus_jitter"),
            (138, "chorus_saturation"),
            (139, "delay_wow"),
            (140, "delay_crossfeed"),
            (141, "reverb_tone"),
            (142, "reverb_chorus"),
            (143, "reverb_bloom"),
            (144, "stereo_depth"),
            (145, "hifi_exciter"),
            (146, "output_transformer"),
            (147, "output_soft_clip"),
            (148, "portamento_time"),
            (150, "fm_decay"),
            (151, "velocity_to_fm"),
            (153, "osc_b_ratio"),
            (154, "osc_c_ratio"),
            (155, "osc_d_ratio"),
            (156, "osc_b_decay"),
            (157, "osc_c_decay"),
            (158, "osc_d_decay"),
            (159, "velocity_to_decay"),
            (160, "key_track_decay"),
        ):
            value = _extended(index)
            if value is not None:
                setattr(patch, attr, value)
        for index, attr in (
            (73, "fm_algorithm"),
            (74, "filter_mode"),
        ):
            value = _extended(index)
            if value is not None:
                setattr(patch, attr, int(round(value)))
        osc_c_waveform = _extended(41)
        if osc_c_waveform is not None:
            patch.oscillator_c = _waveform_from_index(int(round(osc_c_waveform)))
        osc_d_waveform = _extended(61)
        if osc_d_waveform is not None:
            patch.oscillator_d = _waveform_from_index(int(round(osc_d_waveform)))
        osc_c_enabled = _extended(44)
        if osc_c_enabled is not None:
            patch.oscillator_c_enabled = osc_c_enabled >= 0.5
        osc_d_enabled = _extended(62)
        if osc_d_enabled is not None:
            patch.oscillator_d_enabled = osc_d_enabled >= 0.5
        legato = _extended(149)
        if legato is not None:
            patch.portamento_legato = legato >= 0.5
        mono = _extended(152)
        if mono is not None:
            patch.mono_mode = mono >= 0.5
        song.instruments.append(patch)

    _, pattern_header = take("patterns")
    for _ in range(int(pattern_header[1])):
        line_number, tokens = take("pattern")
        if len(tokens) < 4:
            raise ValueError(f"{resolved}:{line_number}: pattern line is incomplete")
        pattern = Pattern(name=tokens[1], rows=int(tokens[2]), tracks=int(tokens[3]))

        while True:
            if cursor >= len(lines):
                raise ValueError(f"{resolved}: pattern {pattern.name!r} is missing end_pattern")
            step_line_number, step_tokens = lines[cursor]
            cursor += 1
            if step_tokens[0] == "end_pattern":
                break
            _expect(step_tokens, "step", resolved, step_line_number)
            if len(step_tokens) < 8:
                raise ValueError(f"{resolved}:{step_line_number}: step line is incomplete")

            row = int(step_tokens[1])
            track = int(step_tokens[2])
            has_note = bool(int(step_tokens[3]))
            step = pattern.step(row, track)
            index = 4
            if has_note:
                step.midi = int(step_tokens[index])
                step.velocity = float(step_tokens[index + 1])
                index += 2
            step.instrument = int(step_tokens[index])
            step.gate = float(step_tokens[index + 1])
            step.micro_offset_rows = float(step_tokens[index + 2])
            automation_count = int(step_tokens[index + 3])
            index += 4
            for _ in range(automation_count):
                step.automation[step_tokens[index]] = float(step_tokens[index + 1])
                index += 2
            if index + 4 < len(step_tokens):
                has_probability = bool(int(step_tokens[index]))
                probability = float(step_tokens[index + 1])
                step.probability = probability if has_probability else None
                step.retrigger_count = int(step_tokens[index + 2])
                step.retrigger_spacing_rows = float(step_tokens[index + 3])
                step.retrigger_velocity_decay = float(step_tokens[index + 4])
                if index + 5 < len(step_tokens):
                    step.note_off = bool(int(step_tokens[index + 5]))
        song.patterns.append(pattern)

    _, order_tokens = take("order")
    expected_order_count = int(order_tokens[1])
    song.order = [int(value) for value in order_tokens[2:]]
    if len(song.order) != expected_order_count:
        raise ValueError(f"{resolved}: order count does not match order entries")
    take("end_project")
    validate_song(song)
    return song


def save_project(song: Song, path: str | Path) -> None:
    validate_song(song)
    lines = [
        f"arachno_project {PROJECT_VERSION}",
        f"title {_quoted(song.title)}",
        f"author {_quoted(song.author)}",
        f"description {_quoted(song.description)}",
        f"notes {_quoted(song.notes)}",
        f"bpm {song.bpm}",
        f"rows_per_beat {song.rows_per_beat}",
        f"sample_rate {song.sample_rate}",
        f"tracks {len(song.tracks)}",
    ]

    for track in song.tracks:
        lines.append(
            f"track {_quoted(track.name)} {track.volume} {track.pan} {_bool(track.muted)} {_bool(track.solo)}"
        )

    lines.append(f"instruments {len(song.instruments)}")
    for index, patch in enumerate(song.instruments):
        lines.append(
            "instrument "
            f"{index} {_quoted(patch.name)} {patch.oscillator_a.value} {patch.oscillator_b.value} "
            f"{_format_values(_patch_project_values(patch))}"
        )

    lines.append(f"patterns {len(song.patterns)}")
    for pattern in song.patterns:
        lines.append(f"pattern {_quoted(pattern.name)} {pattern.rows} {pattern.tracks}")
        for (row, track), step in sorted(pattern.steps.items()):
            if not step.empty:
                lines.append(_serialize_step(row, track, step))
        lines.append("end_pattern")

    lines.append("order " + str(len(song.order)) + "".join(f" {index}" for index in song.order))
    lines.append("end_project")
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def save_patch(patch: SynthPatch, path: str | Path) -> None:
    lines = [
        f"arachno_patch {PATCH_VERSION}",
        f"name {_quoted(patch.name)}",
        f"oscillators {patch.oscillator_a.value} {patch.oscillator_b.value} "
        f"{patch.oscillator_c.value} {patch.oscillator_d.value}",
        f"params {_format_values(_patch_file_values(patch))}",
        f"amp {_format_values(_envelope_values(patch.amp_envelope))}",
        f"filter {_format_values(_envelope_values(patch.filter_envelope))}",
        "end_patch",
    ]
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_patch(path: str | Path) -> SynthPatch:
    """Load a native ``.arachnopatch`` synthesizer preset."""

    resolved, lines = _read_lines(path)
    if len(lines) < 6:
        raise ValueError(f"{resolved}: patch file is incomplete")

    line_number, header = lines[0]
    _expect(header, "arachno_patch", resolved, line_number)
    if len(header) < 2 or int(header[1]) > PATCH_VERSION:
        raise ValueError(f"{resolved}: unsupported patch version")

    line_number, name_tokens = lines[1]
    _expect(name_tokens, "name", resolved, line_number)
    line_number, oscillator_tokens = lines[2]
    _expect(oscillator_tokens, "oscillators", resolved, line_number)
    line_number, params_tokens = lines[3]
    _expect(params_tokens, "params", resolved, line_number)
    line_number, amp_tokens = lines[4]
    _expect(amp_tokens, "amp", resolved, line_number)
    line_number, filter_tokens = lines[5]
    _expect(filter_tokens, "filter", resolved, line_number)

    patch = SynthPatch(
        name=name_tokens[1],
        oscillator_a=_as_waveform(oscillator_tokens[1]),
        oscillator_b=_as_waveform(oscillator_tokens[2]),
        amp_envelope=_parse_envelope(amp_tokens),
        filter_envelope=_parse_envelope(filter_tokens),
    )
    if len(oscillator_tokens) >= 4:
        patch.oscillator_c = _as_waveform(oscillator_tokens[3])
    if len(oscillator_tokens) >= 5:
        patch.oscillator_d = _as_waveform(oscillator_tokens[4])
    params = [float(value) for value in params_tokens[1:]]
    if len(params) >= 102:
        # Modern C++ PatchIO v2 layout (also written by this SDK). The writer is
        # append-only, so any length >= 102 shares the same indices up to its
        # tail: 138-value files (older SDK writer) simply lack the trailing
        # velocity/portamento/fm fields. Fields past index 101 are gated on the
        # same thresholds the C++ reader uses.
        patch.oscillator_b_enabled = params[1] >= 0.5
        patch.oscillator_c_enabled = params[2] >= 0.5
        patch.oscillator_d_enabled = params[3] >= 0.5
        patch.oscillator_mix = params[4]
        patch.oscillator_c_mix = params[5]
        patch.oscillator_d_mix = params[6]
        patch.detune_cents = params[7]
        patch.detune_c_cents = params[8]
        patch.detune_d_cents = params[9]
        patch.pulse_width = params[10]
        patch.pwm_depth = params[11]
        patch.fm_amount = params[13]
        patch.fm_ratio = params[14]
        patch.fm_feedback = params[15]
        patch.fm_algorithm = int(round(params[16]))
        patch.chorus_mix = params[18]
        patch.chorus_rate = params[19]
        patch.chorus_depth = params[20]
        patch.chorus_feedback = params[21]
        patch.chorus_delay = params[22]
        patch.chorus_width = params[23]
        patch.unison_voices = int(round(params[24]))
        patch.unison_detune_cents = params[25]
        patch.stereo_spread = params[26]
        patch.sub_oscillator = params[28]
        patch.noise = params[30]
        patch.noise_tone = params[31]
        patch.cutoff = params[32]
        patch.resonance = params[33]
        patch.filter_mode = int(round(params[34]))
        patch.filter_drive = params[35]
        patch.filter_keytrack = params[36]
        patch.filter_envelope_amount = params[37]
        patch.lfo_filter_depth = params[38]
        patch.lfo_pan_depth = params[39]
        patch.pitch_envelope_semitones = params[40]
        patch.pitch_envelope_decay = params[41]
        patch.lfo_rate = params[42]
        patch.vibrato_cents = params[43]
        patch.tremolo_depth = params[44]
        patch.ring_mod = params[46]
        patch.hard_sync = params[48]
        patch.drive = params[49]
        patch.wavefold = params[50]
        patch.bit_crush = params[52]
        patch.sample_rate_reduction = params[53]
        patch.comb_mix = params[54]
        patch.comb_time = params[55]
        patch.comb_feedback = params[56]
        patch.delay_mix = params[57]
        patch.delay_time = params[58]
        patch.delay_feedback = params[59]
        patch.delay_tone = params[60]
        patch.reverb_mix = params[61]
        patch.reverb_size = params[62]
        patch.reverb_damping = params[63]
        patch.reverb_pre_delay = params[64]
        patch.high_pass = params[65]
        patch.click = params[66]
        patch.transient_shape = params[67]
        patch.transient_noise = params[68]
        patch.transient_pitch_semitones = params[69]
        patch.transient_pitch_decay = params[70]
        patch.transient_burst_count = int(round(params[71]))
        patch.transient_burst_spacing = params[72]
        patch.transient_burst_decay = params[73]
        patch.transient_tone = params[74]
        patch.transient_decay = params[75]
        patch.analog_color = params[76]
        patch.vintage_drift = params[77]
        patch.wow_flutter = params[78]
        patch.tone_tilt = params[79]
        patch.gain = params[80]
        patch.pan = params[81]
        patch.osc_a_level = params[82]
        patch.osc_b_level = params[83]
        patch.osc_c_level = params[84]
        patch.osc_d_level = params[85]
        patch.osc_a_detune_cents = params[86]
        patch.osc_b_detune_cents = params[87]
        patch.osc_c_detune_cents = params[88]
        patch.osc_d_detune_cents = params[89]
        patch.osc_a_pulse_width = params[90]
        patch.osc_b_pulse_width = params[91]
        patch.osc_c_pulse_width = params[92]
        patch.osc_d_pulse_width = params[93]
        patch.osc_a_pwm_depth = params[94]
        patch.osc_b_pwm_depth = params[95]
        patch.osc_c_pwm_depth = params[96]
        patch.osc_d_pwm_depth = params[97]
        patch.osc_a_drive = params[98]
        patch.osc_b_drive = params[99]
        patch.osc_c_drive = params[100]
        patch.osc_d_drive = params[101]
        if len(params) >= 114:
            patch.chorus_ensemble = params[102]
            patch.delay_stereo = params[103]
            patch.delay_mod_depth = params[104]
            patch.delay_drive = params[105]
            patch.delay_ducking = params[106]
            patch.reverb_diffusion = params[107]
            patch.reverb_width = params[108]
            patch.reverb_shimmer = params[109]
            patch.reverb_mod_depth = params[110]
            patch.tape_color = params[111]
            patch.air_boost = params[112]
            patch.low_punch = params[113]
        if len(params) >= 123:
            patch.analog_warmth = params[114]
            patch.voice_slop = params[115]
            patch.phase_scatter = params[116]
            patch.chorus_tone = params[117]
            patch.delay_diffusion = params[118]
            patch.reverb_decay = params[119]
            patch.reverb_early_mix = params[120]
            patch.console_crosstalk = params[121]
            patch.output_glue = params[122]
        if len(params) >= 138:
            patch.unison_warp = params[123]
            patch.unison_humanize = params[124]
            patch.fm_color = params[125]
            patch.fm_spread = params[126]
            patch.chorus_jitter = params[127]
            patch.chorus_saturation = params[128]
            patch.delay_wow = params[129]
            patch.delay_crossfeed = params[130]
            patch.reverb_tone = params[131]
            patch.reverb_chorus = params[132]
            patch.reverb_bloom = params[133]
            patch.stereo_depth = params[134]
            patch.hifi_exciter = params[135]
            patch.output_transformer = params[136]
            patch.output_soft_clip = params[137]
        if len(params) >= 146:
            patch.velocity_to_amp = params[138]
            patch.velocity_to_filter = params[139]
            patch.velocity_to_attack = params[140]
            patch.velocity_curve = int(round(params[141]))
            patch.filter_keytrack_resonance = params[142]
            patch.filter_nonlinearity = params[143]
            patch.amp_envelope_curve = int(round(params[144]))
            patch.filter_envelope_curve = int(round(params[145]))
        if len(params) >= 148:
            patch.portamento_time = params[146]
            patch.portamento_legato = params[147] >= 0.5
        if len(params) >= 151:
            patch.fm_decay = params[148]
            patch.velocity_to_fm = params[149]
            patch.mono_mode = params[150] >= 0.5
        if len(params) >= 159:
            patch.osc_b_ratio = params[151]
            patch.osc_c_ratio = params[152]
            patch.osc_d_ratio = params[153]
            patch.osc_b_decay = params[154]
            patch.osc_c_decay = params[155]
            patch.osc_d_decay = params[156]
            patch.velocity_to_decay = params[157]
            patch.key_track_decay = params[158]
    elif len(params) >= 63:
        patch.detune_cents = params[7]
        patch.pulse_width = params[10]
        patch.pwm_depth = params[11]
        patch.fm_amount = params[13]
        patch.fm_ratio = params[14]
        patch.chorus_mix = params[17]
        patch.chorus_rate = params[18]
        patch.chorus_depth = params[19]
        patch.unison_voices = int(round(params[20]))
        patch.unison_detune_cents = params[21]
        patch.stereo_spread = params[22]
        patch.sub_oscillator = params[24]
        patch.noise = params[26]
        patch.noise_tone = params[27]
        patch.cutoff = params[28]
        patch.resonance = params[29]
        patch.filter_envelope_amount = params[30]
        patch.pitch_envelope_semitones = params[33]
        patch.pitch_envelope_decay = params[34]
        patch.lfo_rate = params[35]
        patch.vibrato_cents = params[36]
        patch.tremolo_depth = params[37]
        patch.ring_mod = params[39]
        patch.hard_sync = params[41]
        patch.drive = params[42]
        patch.bit_crush = params[45]
        patch.sample_rate_reduction = params[46]
        patch.high_pass = params[50]
        patch.click = params[51]
        patch.transient_shape = params[52]
        patch.transient_noise = params[53]
        patch.transient_pitch_semitones = params[54]
        patch.transient_pitch_decay = params[55]
        patch.transient_burst_count = int(round(params[56]))
        patch.transient_burst_spacing = params[57]
        patch.transient_burst_decay = params[58]
        patch.transient_tone = params[59]
        patch.transient_decay = params[60]
        patch.gain = params[61]
        patch.pan = params[62]
    elif len(params) >= 55:
        patch.detune_cents = params[7]
        patch.pulse_width = params[10]
        patch.pwm_depth = params[11]
        patch.fm_amount = params[13]
        patch.fm_ratio = params[14]
        patch.chorus_mix = params[17]
        patch.chorus_rate = params[18]
        patch.chorus_depth = params[19]
        patch.unison_voices = int(round(params[20]))
        patch.unison_detune_cents = params[21]
        patch.stereo_spread = params[22]
        patch.sub_oscillator = params[24]
        patch.noise = params[26]
        patch.cutoff = params[27]
        patch.resonance = params[28]
        patch.filter_envelope_amount = params[29]
        patch.pitch_envelope_semitones = params[32]
        patch.pitch_envelope_decay = params[33]
        patch.lfo_rate = params[34]
        patch.vibrato_cents = params[35]
        patch.tremolo_depth = params[36]
        patch.ring_mod = params[38]
        patch.hard_sync = params[40]
        patch.drive = params[41]
        patch.bit_crush = params[44]
        patch.sample_rate_reduction = params[45]
        patch.high_pass = params[49]
        patch.click = params[50]
        patch.transient_noise = params[51]
        patch.transient_decay = params[52]
        patch.gain = params[53]
        patch.pan = params[54]
    else:
        _apply_fields(patch, _PATCH_FILE_FIELDS, params_tokens[1:])
    return patch
