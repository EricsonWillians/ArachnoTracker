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
            self.assertIn("arachno_patch 1", patch_text)
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
