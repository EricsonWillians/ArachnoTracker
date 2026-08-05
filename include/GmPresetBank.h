#pragma once

#include <string>

#include "Instrument.h"

namespace arachno {

// Curated General MIDI preset bank: 128 hand-tuned SynthPatch presets targeting
// professional 80s/90s synth character (FM electric pianos, Juno-style ensemble
// chorus strings/pads, analog mono basses with glide, sync leads, FM bells).
// Used by the MIDI importer to voice imported .mid files and exportable to
// .arachnopatch files via `--write-gm-presets <dir>` for the GUI patch browser.
//
// program is the 0-based GM program number (0..127); averageMidi is the mean note
// of the imported lane, used for key-scaled envelope compensation.
SynthPatch gmPresetForProgram(int program, double averageMidi);

// GM percussion (channel 10): lanes are split per drum class (kick/snare/clap/
// hat/tom/cymbal/perc) by the importer, and each class gets its own tuned patch.
// drumClassForNote maps a GM note number to a class index (0..6);
// drumClassName gives the display name; gmDrumClassPreset builds the patch.
int drumClassForNote(int midiNote);
const char* drumClassName(int drumClass);
SynthPatch gmDrumClassPreset(int drumClass);

// Standard GM program name for program 0..127.
const char* gmProgramName(int program);

} // namespace arachno
