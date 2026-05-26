NOTE_NAMES = {
    "C": 0,
    "C#": 1,
    "DB": 1,
    "D": 2,
    "D#": 3,
    "EB": 3,
    "E": 4,
    "F": 5,
    "F#": 6,
    "GB": 6,
    "G": 7,
    "G#": 8,
    "AB": 8,
    "A": 9,
    "A#": 10,
    "BB": 10,
    "B": 11,
}

SHARP_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name_to_midi(name: str) -> int:
    token = name.strip().upper()
    if len(token) < 2:
        raise ValueError(f"invalid note name: {name}")

    if len(token) >= 2 and token[1] in {"#", "B"}:
        note = token[:2]
        octave_text = token[2:]
    else:
        note = token[:1]
        octave_text = token[1:]

    if note not in NOTE_NAMES or not octave_text:
        raise ValueError(f"invalid note name: {name}")
    midi = (int(octave_text) + 1) * 12 + NOTE_NAMES[note]
    if midi < 0 or midi > 127:
        raise ValueError(f"midi note out of range: {name}")
    return midi


def midi_to_note_name(midi: int) -> str:
    if midi < 0 or midi > 127:
        raise ValueError(f"midi note out of range: {midi}")
    return f"{SHARP_NAMES[midi % 12]}{midi // 12 - 1}"
