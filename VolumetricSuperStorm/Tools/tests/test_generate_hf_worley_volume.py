"""Contract tests for the v10 raw-octave HF Worley generator."""

from __future__ import annotations

import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock


GENERATOR_PATH = Path(__file__).parents[1] / "GenerateHFWorleyVolume.py"
SPEC = importlib.util.spec_from_file_location("generate_hf_worley_volume", GENERATOR_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot import HF generator from {GENERATOR_PATH}")
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class GenerateHFWorleyVolumeTests(unittest.TestCase):
    def test_rgb_contains_direct_o0_o1_o2_samples(self) -> None:
        size = 16
        voxel_count = size**3
        raw_octaves = {
            2: bytearray([23]) * voxel_count,
            4: bytearray([101]) * voxel_count,
            8: bytearray([211]) * voxel_count,
        }

        with mock.patch.object(
            generator,
            "_generate_inverted_worley_octave",
            side_effect=lambda _size, cells, _seed: raw_octaves[cells],
        ) as generate_octave:
            rgba = generator.generate_volume(size=size, seed=7)

        self.assertEqual(
            [call.args[1] for call in generate_octave.call_args_list],
            list(generator.HF_OCTAVE_FREQUENCIES),
        )
        self.assertEqual(rgba[:4], bytes((23, 101, 211, 255)))
        self.assertEqual(
            rgba,
            bytes((23, 101, 211, 255)) * voxel_count,
            "RGB must contain the raw octaves without overlapping bands or weights",
        )

    def test_generated_octaves_are_ordered_low_to_high_frequency(self) -> None:
        size = 16
        rgba = generator.generate_volume(size=size, seed=12345)

        self.assertEqual(len(rgba), size**3 * 4)
        self.assertTrue(all(rgba[index] == 255 for index in range(3, len(rgba), 4)))
        deltas = generator.validate_volume(rgba, size)
        self.assertLess(deltas[0], deltas[1])
        self.assertLess(deltas[1], deltas[2])

    def test_generator_is_deterministic_for_seed(self) -> None:
        first = generator.generate_volume(size=16, seed=91)
        second = generator.generate_volume(size=16, seed=91)
        different = generator.generate_volume(size=16, seed=92)

        self.assertEqual(first, second)
        self.assertNotEqual(first, different)

    def test_weights_cli_is_rejected(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                generator._parse_args(["--weights", "0.625", "0.250", "0.125"])

    def test_cli_writes_valid_raw_octave_dds(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "hf_raw_octaves.dds"
            with contextlib.redirect_stdout(io.StringIO()):
                exit_code = generator.main(
                    [str(output), "--size", "16", "--seed", "314159"],
                )

            payload = output.read_bytes()
            self.assertEqual(exit_code, 0)
            self.assertEqual(payload[:4], b"DDS ")
            self.assertEqual(len(payload), 148 + 16**3 * 4)
            generator.validate_volume(payload[148:], 16)


if __name__ == "__main__":
    unittest.main()
