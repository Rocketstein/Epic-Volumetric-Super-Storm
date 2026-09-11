import math
import pathlib


payload = pathlib.Path(__file__).with_name("VT_WorleyDetail_32.dds").read_bytes()[148:]
width = height = depth = 32

# DDS is DXGI_FORMAT_B8G8R8A8_UNORM (87); shader .rgb maps to bytes 2,1,0.
hf = [
    (payload[index + 2] * 0.625 + payload[index + 1] * 0.250 + payload[index] * 0.125) / 255.0
    for index in range(0, len(payload), 4)
]
ordered = sorted(hf)
mean = sum(hf) / len(hf)
std = math.sqrt(sum((value - mean) ** 2 for value in hf) / len(hf))
quantiles = [ordered[round((len(ordered) - 1) * q)] for q in (0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)]
print(
    "HF_FBM_SWIZZLED",
    f"min={ordered[0]:.6f}",
    f"max={ordered[-1]:.6f}",
    f"mean={mean:.6f}",
    f"std={std:.6f}",
    "q=" + ",".join(f"{value:.6f}" for value in quantiles),
)

deltas = {"x": [], "y": [], "z": []}
for z in range(depth):
    for y in range(height):
        for x in range(width):
            i = (z * height + y) * width + x
            if x + 1 < width:
                deltas["x"].append(abs(hf[i] - hf[i + 1]))
            if y + 1 < height:
                deltas["y"].append(abs(hf[i] - hf[i + width]))
            if z + 1 < depth:
                deltas["z"].append(abs(hf[i] - hf[i + width * height]))
print("HF_DELTA_SWIZZLED", " ".join(f"{axis}={sum(v) / len(v):.6f}" for axis, v in deltas.items()))

