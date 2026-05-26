from __future__ import annotations

import json
import shlex
from pathlib import Path
from typing import Callable, Iterable, Sequence

from .model import Envelope, Pattern, Song, Step, SynthPatch, Track, Waveform

PROJECT_VERSION = 1
PATCH_VERSION = 1

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
    return (
        patch.oscillator_mix,
        patch.detune_cents,
        patch.sub_oscillator,
        patch.noise,
        patch.cutoff,
        patch.resonance,
        patch.filter_envelope_amount,
        patch.lfo_rate,
        patch.vibrato_cents,
        patch.tremolo_depth,
        patch.drive,
        patch.gain,
        patch.pan,
        *_envelope_values(patch.amp_envelope),
        *_envelope_values(patch.filter_envelope),
        patch.pitch_envelope_semitones,
        patch.pitch_envelope_decay,
        patch.ring_mod,
        patch.hard_sync,
        patch.bit_crush,
        patch.sample_rate_reduction,
        patch.high_pass,
        patch.unison_voices,
        patch.unison_detune_cents,
        patch.stereo_spread,
        patch.click,
        patch.transient_noise,
        patch.transient_decay,
        patch.pulse_width,
        patch.pwm_depth,
        patch.fm_amount,
        patch.fm_ratio,
        patch.chorus_mix,
        patch.chorus_rate,
        patch.chorus_depth,
    )


def _patch_file_values(patch: SynthPatch) -> Iterable[object]:
    return (
        patch.oscillator_mix,
        patch.detune_cents,
        patch.pulse_width,
        patch.pwm_depth,
        patch.fm_amount,
        patch.fm_ratio,
        patch.chorus_mix,
        patch.chorus_rate,
        patch.chorus_depth,
        patch.unison_voices,
        patch.unison_detune_cents,
        patch.stereo_spread,
        patch.sub_oscillator,
        patch.noise,
        patch.cutoff,
        patch.resonance,
        patch.filter_envelope_amount,
        patch.pitch_envelope_semitones,
        patch.pitch_envelope_decay,
        patch.lfo_rate,
        patch.vibrato_cents,
        patch.tremolo_depth,
        patch.ring_mod,
        patch.hard_sync,
        patch.drive,
        patch.bit_crush,
        patch.sample_rate_reduction,
        patch.high_pass,
        patch.click,
        patch.transient_noise,
        patch.transient_decay,
        patch.gain,
        patch.pan,
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
        f"oscillators {patch.oscillator_a.value} {patch.oscillator_b.value}",
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
    params = [float(value) for value in params_tokens[1:]]
    if len(params) >= 63:
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
