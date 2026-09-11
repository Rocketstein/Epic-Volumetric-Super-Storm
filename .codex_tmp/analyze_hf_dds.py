import collections
import math
import pathlib
import struct


path = pathlib.Path(__file__).with_name("VT_WorleyDetail_32.dds")
data = path.read_bytes()
if data[:4] != b"DDS ":
    raise RuntimeError("Not a DDS file")

height, width, pitch_or_linear, depth, mip_count = struct.unpack_from("<5I", data, 12)
fourcc = data[84:88]
dxgi_format = struct.unpack_from("<I", data, 128)[0] if fourcc == b"DX10" else None
resource_dimension = struct.unpack_from("<I", data, 132)[0] if fourcc == b"DX10" else None
array_size = struct.unpack_from("<I", data, 140)[0] if fourcc == b"DX10" else None
payload_offset = 148 if fourcc == b"DX10" else 128
payload = data[payload_offset:]

print(
    "HEADER",
    f"width={width}",
    f"height={height}",
    f"depth={depth}",
    f"mips={mip_count}",
    f"pitch={pitch_or_linear}",
    f"fourcc={fourcc!r}",
    f"dxgi={dxgi_format}",
    f"dimension={resource_dimension}",
    f"array={array_size}",
    f"payload={len(payload)}",
)

voxel_count = width * height * depth
if len(payload) != voxel_count * 4:
    raise RuntimeError(f"Expected {voxel_count * 4} raw bytes, got {len(payload)}")

channels = [[payload[index + channel] for index in range(0, len(payload), 4)] for channel in range(4)]
for channel_index, values in enumerate(channels):
    ordered = sorted(values)
    mean = sum(values) / voxel_count
    variance = sum((value - mean) ** 2 for value in values) / voxel_count
    entropy = -sum(
        (count / voxel_count) * math.log2(count / voxel_count)
        for count in collections.Counter(values).values()
    )
    quantiles = [ordered[round((voxel_count - 1) * q)] for q in (0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)]
    print(
        "CHANNEL",
        channel_index,
        f"min={ordered[0]}",
        f"max={ordered[-1]}",
        f"mean={mean:.4f}",
        f"std={math.sqrt(variance):.4f}",
        f"unique={len(set(values))}",
        f"entropy={entropy:.4f}",
        "q=" + ",".join(map(str, quantiles)),
    )


def voxel(channel, x, y, z):
    return channels[channel][(z * height + y) * width + x]


for channel in range(3):
    deltas = {"x": [], "y": [], "z": []}
    for z in range(depth):
        for y in range(height):
            for x in range(width):
                value = voxel(channel, x, y, z)
                if x + 1 < width:
                    deltas["x"].append(abs(value - voxel(channel, x + 1, y, z)))
                if y + 1 < height:
                    deltas["y"].append(abs(value - voxel(channel, x, y + 1, z)))
                if z + 1 < depth:
                    deltas["z"].append(abs(value - voxel(channel, x, y, z + 1)))
    print(
        "DELTA",
        channel,
        " ".join(
            f"{axis}_mean={sum(values) / len(values):.4f} {axis}_nonzero={sum(v != 0 for v in values) / len(values):.4f}"
            for axis, values in deltas.items()
        ),
    )

# The material consumes dot(rgb, [0.625, 0.250, 0.125]).
weights = (0.625, 0.250, 0.125)
hf = [sum(channels[channel][i] * weights[channel] for channel in range(3)) / 255.0 for i in range(voxel_count)]
hf_sorted = sorted(hf)
hf_mean = sum(hf) / voxel_count
hf_variance = sum((value - hf_mean) ** 2 for value in hf) / voxel_count
hf_quantiles = [hf_sorted[round((voxel_count - 1) * q)] for q in (0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)]
print(
    "HF_FBM",
    f"min={hf_sorted[0]:.6f}",
    f"max={hf_sorted[-1]:.6f}",
    f"mean={hf_mean:.6f}",
    f"std={math.sqrt(hf_variance):.6f}",
    "q=" + ",".join(f"{value:.6f}" for value in hf_quantiles),
)

hf_delta = {"x": [], "y": [], "z": []}
for z in range(depth):
    for y in range(height):
        for x in range(width):
            index = (z * height + y) * width + x
            value = hf[index]
            if x + 1 < width:
                hf_delta["x"].append(abs(value - hf[index + 1]))
            if y + 1 < height:
                hf_delta["y"].append(abs(value - hf[index + width]))
            if z + 1 < depth:
                hf_delta["z"].append(abs(value - hf[index + width * height]))
print(
    "HF_DELTA",
    " ".join(f"{axis}_mean={sum(values) / len(values):.6f}" for axis, values in hf_delta.items()),
)



