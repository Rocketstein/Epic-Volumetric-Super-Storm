#!/usr/bin/env python3
"""Generate the SavageSuperStorm raw-octave HF volume texture.

The output is an uncompressed R8G8B8A8_UNORM Texture3D DDS. RGB contains
three independent, periodic inverted-Worley octaves in ascending frequency;
alpha is opaque. Composition weights belong exclusively to shader decode.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import random
import struct
from typing import Sequence


HF_OCTAVE_FREQUENCIES = (2, 4, 8)
DEFAULT_SIZE = 32
DEFAULT_SEED = 0x535353


def _feature_points(cells: int, seed: int) -> list[tuple[float, float, float]]:
    rng = random.Random(seed)
    return [
        (
            x + rng.random(),
            y + rng.random(),
            z + rng.random(),
        )
        for z in range(cells)
        for y in range(cells)
        for x in range(cells)
    ]


def _feature_index(x: int, y: int, z: int, cells: int) -> int:
    return (z * cells + y) * cells + x


def _generate_inverted_worley_octave(
    size: int,
    cells: int,
    seed: int,
) -> bytearray:
    features = _feature_points(cells, seed)
    result = bytearray(size * size * size)

    for z in range(size):
        pz = (z + 0.5) * cells / size
        cell_z = math.floor(pz)
        for y in range(size):
            py = (y + 0.5) * cells / size
            cell_y = math.floor(py)
            for x in range(size):
                px = (x + 0.5) * cells / size
                cell_x = math.floor(px)
                nearest_squared = float("inf")

                for dz in (-1, 0, 1):
                    unwrapped_z = cell_z + dz
                    wrapped_z = unwrapped_z % cells
                    shift_z = unwrapped_z - wrapped_z
                    for dy in (-1, 0, 1):
                        unwrapped_y = cell_y + dy
                        wrapped_y = unwrapped_y % cells
                        shift_y = unwrapped_y - wrapped_y
                        for dx in (-1, 0, 1):
                            unwrapped_x = cell_x + dx
                            wrapped_x = unwrapped_x % cells
                            shift_x = unwrapped_x - wrapped_x
                            fx, fy, fz = features[_feature_index(
                                wrapped_x,
                                wrapped_y,
                                wrapped_z,
                                cells,
                            )]
                            delta_x = px - (fx + shift_x)
                            delta_y = py - (fy + shift_y)
                            delta_z = pz - (fz + shift_z)
                            nearest_squared = min(
                                nearest_squared,
                                delta_x * delta_x
                                + delta_y * delta_y
                                + delta_z * delta_z,
                            )

                inverted = 1.0 - min(math.sqrt(nearest_squared), 1.0)
                index = (z * size + y) * size + x
                result[index] = round(inverted * 255.0)

    return result


def generate_volume(
    size: int = DEFAULT_SIZE,
    seed: int = DEFAULT_SEED,
) -> bytes:
    """Return direct O0/O1/O2/1 RGBA voxels in x-fastest order."""
    if size < 2 * HF_OCTAVE_FREQUENCIES[-1]:
        raise ValueError(
            f"size must be at least {2 * HF_OCTAVE_FREQUENCIES[-1]} "
            "to resolve the highest octave",
        )

    octaves = [
        _generate_inverted_worley_octave(size, frequency, seed + index * 7919)
        for index, frequency in enumerate(HF_OCTAVE_FREQUENCIES)
    ]
    rgba = bytearray(size * size * size * 4)
    for voxel_index in range(size * size * size):
        rgba[voxel_index * 4 + 0] = octaves[0][voxel_index]
        rgba[voxel_index * 4 + 1] = octaves[1][voxel_index]
        rgba[voxel_index * 4 + 2] = octaves[2][voxel_index]
        rgba[voxel_index * 4 + 3] = 255
    return bytes(rgba)


def _mean_neighbor_delta(
    rgba: bytes,
    size: int,
    channel: int,
) -> float:
    total = 0
    edge_count = 0

    def sample(x: int, y: int, z: int) -> int:
        return rgba[((z * size + y) * size + x) * 4 + channel]

    for z in range(size):
        for y in range(size):
            for x in range(size):
                value = sample(x, y, z)
                total += abs(value - sample((x + 1) % size, y, z))
                total += abs(value - sample(x, (y + 1) % size, z))
                total += abs(value - sample(x, y, (z + 1) % size))
                edge_count += 3
    return total / edge_count


def validate_volume(rgba: bytes, size: int) -> tuple[float, float, float]:
    expected_bytes = size * size * size * 4
    if len(rgba) != expected_bytes:
        raise ValueError(f"expected {expected_bytes} RGBA bytes, got {len(rgba)}")
    if any(rgba[index] != 255 for index in range(3, len(rgba), 4)):
        raise ValueError("alpha must be exactly one for every voxel")

    deltas = tuple(_mean_neighbor_delta(rgba, size, channel) for channel in range(3))
    if not deltas[0] < deltas[1] < deltas[2]:
        raise ValueError(
            "measured octave ordering must be R < G < B; "
            f"neighbor deltas were {deltas}",
        )
    return deltas


def _dds_header(size: int) -> bytes:
    # DDS_HEADER with a DX10 R8G8B8A8_UNORM Texture3D extension.
    ddsd_caps = 0x1
    ddsd_height = 0x2
    ddsd_width = 0x4
    ddsd_pitch = 0x8
    ddsd_pixel_format = 0x1000
    ddsd_depth = 0x800000
    header_flags = (
        ddsd_caps
        | ddsd_height
        | ddsd_width
        | ddsd_pitch
        | ddsd_pixel_format
        | ddsd_depth
    )
    pixel_format = struct.pack(
        "<II4sIIIII",
        32,
        0x4,  # DDPF_FOURCC
        b"DX10",
        0,
        0,
        0,
        0,
        0,
    )
    header = struct.pack(
        "<IIIIIII11I",
        124,
        header_flags,
        size,
        size,
        size * 4,
        size,
        1,
        *([0] * 11),
    )
    header += pixel_format
    header += struct.pack(
        "<IIIII",
        0x1000,  # DDSCAPS_TEXTURE
        0x200000,  # DDSCAPS2_VOLUME
        0,
        0,
        0,
    )
    dx10_header = struct.pack(
        "<IIIII",
        28,  # DXGI_FORMAT_R8G8B8A8_UNORM
        4,  # D3D10_RESOURCE_DIMENSION_TEXTURE3D
        0,
        1,
        0,
    )
    return b"DDS " + header + dx10_header


def write_dds(path: Path, rgba: bytes, size: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(_dds_header(size) + rgba)


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        default=Path(f"VT_WorleyDetail_{DEFAULT_SIZE}.dds"),
    )
    parser.add_argument("--size", type=int, default=DEFAULT_SIZE)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv)
    rgba = generate_volume(args.size, args.seed)
    deltas = validate_volume(rgba, args.size)
    write_dds(args.output, rgba, args.size)
    print(
        f"wrote {args.output} ({args.size}^3 RGBA8); "
        f"mean neighbor deltas R/G/B={deltas[0]:.3f}/"
        f"{deltas[1]:.3f}/{deltas[2]:.3f}",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
