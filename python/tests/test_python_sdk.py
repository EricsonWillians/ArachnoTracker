import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest import mock

import arachnotracker as at
from arachnotracker.__main__ import main as sdk_main


class PythonSdkTests(unittest.TestCase):
    def test_note_conversion(self):
        self.assertEqual(at.note_name_to_midi("C4"), 60)
        self.assertEqual(at.note_name_to_midi("A4"), 69)
        self.assertEqual(at.midi_to_note_name(60), "C4")

    def test_project_writer_and_patch_plugin(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            plugin = tmp_path / "industrial_bass.py"
            plugin.write_text(
                "from arachnotracker import SynthPatch, Waveform\n"
                "def create_patch(name='Plugin Bass'):\n"
                "    return SynthPatch(name=name, oscillator_a=Waveform.SAW, oscillator_b=Waveform.SQUARE, drive=0.4)\n",
                encoding="utf-8",
            )

            song = at.Song(
                title="Python EBM Sketch",
                author="SDK Test",
                bpm=132,
                rows_per_beat=4,
            )
            drums = song.add_track("Drums", volume=0.95)
            bass = song.add_track("Bass", volume=0.88)
            kick = song.add_instrument(at.presets.ebm_kick())
            hat = song.add_instrument(at.presets.metal_hat())
            bass_patch = song.add_instrument(at.load_patch_plugin(plugin, name="Plugin Bass"))

            pattern = at.Pattern("Python Pattern", rows=16, tracks=len(song.tracks))
            for row in (0, 4, 8, 12):
                step = pattern.step(row, drums)
                step.midi = at.note_name_to_midi("C2")
                step.velocity = 0.95
                step.instrument = kick
                step.gate = 0.3
            for row in (2, 6, 10, 14):
                step = pattern.step(row, drums)
                step.midi = at.note_name_to_midi("C5")
                step.velocity = 0.4
                step.instrument = hat
                step.probability = 0.75
                step.retrigger_count = 3
                step.retrigger_spacing_rows = 0.12
                step.retrigger_velocity_decay = 0.7

            bass_step = pattern.step(0, bass)
            bass_step.midi = at.note_name_to_midi("C2")
            bass_step.velocity = 0.85
            bass_step.instrument = bass_patch
            bass_step.automation["cutoff"] = 0.42

            song.add_pattern(pattern)
            song.order = [0, 0]

            project = tmp_path / "python_sketch.arachno"
            patch = tmp_path / "plugin_bass.arachnopatch"
            at.save_project(song, project)
            at.save_patch(song.instruments[bass_patch], patch)
            loaded_song = at.load_project(project)
            loaded_patch = at.load_patch(patch)

            project_text = project.read_text(encoding="utf-8")
            patch_text = patch.read_text(encoding="utf-8")
            self.assertIn("arachno_project 1", project_text)
            self.assertIn('"Python EBM Sketch"', project_text)
            self.assertIn(" 1 0.75 3 0.12 0.7", project_text)
            self.assertIn("arachno_patch 2", patch_text)
            self.assertIn('"Plugin Bass"', patch_text)
            self.assertEqual(loaded_song.title, "Python EBM Sketch")
            self.assertEqual(loaded_song.tracks[drums].name, "Drums")
            self.assertEqual(loaded_song.instruments[bass_patch].name, "Plugin Bass")
            self.assertEqual(loaded_song.instruments[bass_patch].drive, 0.4)
            self.assertEqual(loaded_song.order, [0, 0])
            loaded_hat = loaded_song.patterns[0].steps[(2, drums)]
            self.assertEqual(loaded_hat.probability, 0.75)
            self.assertEqual(loaded_hat.retrigger_count, 3)
            self.assertEqual(loaded_hat.retrigger_spacing_rows, 0.12)
            self.assertEqual(loaded_song.patterns[0].steps[(0, bass)].automation["cutoff"], 0.42)
            self.assertEqual(loaded_patch.name, "Plugin Bass")
            self.assertEqual(loaded_patch.oscillator_a, at.Waveform.SAW)
            self.assertEqual(loaded_patch.drive, 0.4)

    def test_note_off_step_and_string_presets(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            song = at.Song(title="Note Off", bpm=120, rows_per_beat=4)
            track = song.add_track("Lead")
            instrument = song.add_instrument(at.presets.classic_strings())
            pattern = at.Pattern("P", rows=16, tracks=len(song.tracks))
            note = pattern.step(0, track)
            note.midi = at.note_name_to_midi("C4")
            note.instrument = instrument
            off = pattern.step(8, track)
            off.note_off = True
            song.add_pattern(pattern)
            song.order = [0]

            project = tmp_path / "noteoff.arachno"
            at.save_project(song, project)
            loaded = at.load_project(project)
            loaded_off = loaded.patterns[0].steps[(8, track)]
            self.assertTrue(loaded_off.note_off)
            self.assertIsNone(loaded_off.midi)
            self.assertFalse(loaded_off.empty)
            self.assertFalse(loaded.patterns[0].steps[(0, track)].note_off)

            # Legacy step lines without the trailing note-off token must still load.
            legacy_lines = []
            for line in project.read_text(encoding="utf-8").splitlines():
                if line.startswith("step 8 "):
                    line = line.rsplit(" ", 1)[0]
                legacy_lines.append(line)
            legacy = tmp_path / "legacy.arachno"
            legacy.write_text("\n".join(legacy_lines) + "\n", encoding="utf-8")
            legacy_loaded = at.load_project(legacy)
            self.assertFalse(legacy_loaded.patterns[0].steps[(8, track)].note_off)

            for builder in (
                at.presets.classic_strings,
                at.presets.synth_strings_85,
                at.presets.analog_string_machine,
            ):
                patch = builder()
                self.assertGreater(patch.amp_envelope.sustain, 0.5)
                self.assertGreater(patch.amp_envelope.release, 0.3)
                patch_path = tmp_path / f"{patch.name}.arachnopatch"
                at.save_patch(patch, patch_path)
                self.assertEqual(at.load_patch(patch_path).name, patch.name)

    def test_extended_patch_fields_round_trip(self):
        # Non-default values across the newly exposed engine surface (must
        # survive both the .arachnopatch and the .arachno project round-trips).
        expected = {
            "oscillator_c": at.Waveform.SAW,
            "oscillator_c_enabled": True,
            "oscillator_c_mix": 0.4,
            "detune_c_cents": -11.0,
            "oscillator_d": at.Waveform.TRIANGLE,
            "oscillator_d_enabled": True,
            "oscillator_d_mix": 0.31,
            "detune_d_cents": 9.0,
            "osc_b_ratio": 3.98,
            "osc_c_ratio": 0.51,
            "osc_d_ratio": 2.02,
            "osc_b_decay": 0.12,
            "osc_c_decay": 0.23,
            "osc_d_decay": 0.34,
            "velocity_to_decay": 0.6,
            "key_track_decay": 0.7,
            "analog_color": 0.11,
            "reverb_tone": 0.33,
            "filter_mode": 3,
            "fm_feedback": 0.22,
            "fm_algorithm": 2,
            "wavefold": 0.17,
            "comb_mix": 0.29,
            "delay_tone": 0.71,
            "chorus_feedback": 0.19,
            "osc_b_level": 0.62,
            "osc_c_pulse_width": 0.61,
            "osc_d_pwm_depth": 0.14,
            "osc_a_drive": 0.09,
            "vintage_drift": 0.21,
            "tone_tilt": 0.13,
            "delay_ducking": 0.18,
            "reverb_shimmer": 0.27,
            "tape_color": 0.37,
            "unison_warp": 0.29,
            "fm_color": 0.63,
            "chorus_jitter": 0.31,
            "delay_wow": 0.11,
            "reverb_bloom": 0.43,
            "stereo_depth": 0.39,
            "output_transformer": 0.28,
            "output_soft_clip": 0.44,
        }
        # Patch-file-only fields (the project instrument chain omits the
        # velocity expression block by design).
        patch_file_only = {
            "velocity_to_amp": 0.5,
            "velocity_to_filter": 0.41,
            "velocity_to_attack": 0.32,
            "velocity_curve": 2,
            "filter_keytrack_resonance": 0.19,
            "filter_nonlinearity": 0.47,
            "amp_envelope_curve": 1,
            "filter_envelope_curve": 2,
        }
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            patch = at.SynthPatch(name="Extended Round Trip", **expected, **patch_file_only)

            patch_path = tmp_path / "extended.arachnopatch"
            at.save_patch(patch, patch_path)
            loaded_patch = at.load_patch(patch_path)
            for attr, value in {**expected, **patch_file_only}.items():
                self.assertEqual(getattr(loaded_patch, attr), value, f"patch file field {attr}")

            song = at.Song(title="Extended", bpm=120, rows_per_beat=4)
            track = song.add_track("Lead")
            instrument = song.add_instrument(patch)
            pattern = at.Pattern("P", rows=8, tracks=len(song.tracks))
            step = pattern.step(0, track)
            step.midi = at.note_name_to_midi("C4")
            step.instrument = instrument
            song.add_pattern(pattern)
            song.order = [0]

            project_path = tmp_path / "extended.arachno"
            at.save_project(song, project_path)
            loaded_song = at.load_project(project_path)
            loaded_instrument = loaded_song.instruments[instrument]
            for attr, value in expected.items():
                self.assertEqual(getattr(loaded_instrument, attr), value, f"project field {attr}")

    def test_edit_script_writer(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "commands.arachno-edit"
            script = (
                at.EditScript()
                .move(14, 1)
                .probability(0.65)
                .retrigger(3, 0.12, 0.72)
                .automation("cutoff", 0.8)
            )
            script.write(path)
            self.assertEqual(
                path.read_text(encoding="utf-8").splitlines(),
                [
                    "move 14 1",
                    "probability 0.65",
                    "retrig 3 0.12 0.72",
                    "param cutoff 0.8",
                ],
            )

    def test_generative_helpers_and_template(self):
        self.assertEqual(
            at.generative.scale_notes("C4", "minor", [0, 1, 2, 3]),
            [60, 62, 63, 65],
        )
        self.assertEqual(at.generative.euclidean_hits(8, 3), [True, False, False, True, False, False, True, False])

        song = at.darkwave_ebm_starter("Community Starter", bpm=128)
        self.assertEqual(song.title, "Community Starter")
        self.assertEqual(song.bpm, 128)
        self.assertGreaterEqual(len(song.tracks), 4)
        self.assertGreaterEqual(len(song.instruments), 6)
        self.assertEqual(song.order, [0, 0])

        pattern = song.patterns[0]
        retriggered = [
            step
            for step in pattern.steps.values()
            if step.retrigger_count > 1 or step.probability is not None
        ]
        self.assertTrue(retriggered)

    def test_python_module_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            project = tmp_path / "starter.arachno"
            patch = tmp_path / "plugin.arachnopatch"
            plugin = tmp_path / "plugin.py"
            plugin.write_text(
                "from arachnotracker import SynthPatch, Waveform\n"
                "def create_patch(name='CLI Patch'):\n"
                "    return SynthPatch(name=name, oscillator_a=Waveform.SAW, drive=0.5)\n",
                encoding="utf-8",
            )

            with mock.patch("sys.argv", ["arachnotracker", "new-ebm", str(project), "--title", "CLI Song"]):
                with redirect_stdout(StringIO()):
                    self.assertEqual(sdk_main(), 0)
            self.assertIn('"CLI Song"', project.read_text(encoding="utf-8"))

            with mock.patch(
                "sys.argv",
                ["arachnotracker", "patch-plugin", str(plugin), str(patch), "--name", "CLI Bass"],
            ):
                with redirect_stdout(StringIO()):
                    self.assertEqual(sdk_main(), 0)
            self.assertIn('"CLI Bass"', patch.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
