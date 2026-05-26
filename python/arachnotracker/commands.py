from __future__ import annotations

from pathlib import Path
from typing import List


class EditScript:
    """Build an ArachnoTracker command file from Python."""

    def __init__(self) -> None:
        self.commands: List[str] = []

    def add(self, command: str) -> "EditScript":
        self.commands.append(command)
        return self

    def move(self, row: int, track: int) -> "EditScript":
        return self.add(f"move {row} {track}")

    def note(self, name: str, velocity: float = 1.0) -> "EditScript":
        return self.add(f"note {name} {velocity}")

    def instrument(self, index: int) -> "EditScript":
        return self.add(f"inst {index}")

    def gate(self, rows: float) -> "EditScript":
        return self.add(f"gate {rows}")

    def probability(self, value: float | None) -> "EditScript":
        return self.add("probability clear" if value is None else f"probability {value}")

    def retrigger(self, count: int, spacing_rows: float = 0.25, velocity_decay: float = 0.85) -> "EditScript":
        return self.add(f"retrig {count} {spacing_rows} {velocity_decay}")

    def automation(self, parameter: str, value: float) -> "EditScript":
        return self.add(f"param {parameter} {value}")

    def write(self, path: str | Path) -> None:
        Path(path).write_text("\n".join(self.commands) + "\n", encoding="utf-8")
