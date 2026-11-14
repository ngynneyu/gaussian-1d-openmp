"""Check the built Sequential and OpenMP executables on small known images."""

import argparse
import math
from pathlib import Path
import struct
import subprocess
from tempfile import TemporaryDirectory


def expect(condition, context):
    if not condition:
        raise AssertionError(context)


def reference(width, height, pixels, size, sigma):
    radius = size // 2
    offsets = [
        (dx, dy, math.exp(-(dx * dx + dy * dy) / (2 * sigma * sigma)))
        for dy in range(-radius, radius + 1)
        for dx in range(-radius, radius + 1)
    ]
    norm = math.fsum(weight for _, _, weight in offsets)
    output = []
    for y in range(height):
        for x in range(width):
            terms = []
            for dx, dy, weight in offsets:
                sx = min(width - 1, max(0, x - dx))
                sy = min(height - 1, max(0, y - dy))
                terms.append(weight * pixels[sy * width + sx])
            output.append(math.floor(math.fsum(terms) / norm + 0.5))
    return bytes(output)


def run(binary, *args, success=True):
    result = subprocess.run([str(binary), *map(str, args)], capture_output=True, text=True)
    expect((result.returncode == 0) == success, f"{binary.name} {args}: {result.stderr}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sequential", type=Path)
    parser.add_argument("openmp", type=Path)
    parser.add_argument("--threads", nargs="+", type=int, default=(2, 4, 8, 16, 20))
    args = parser.parse_args()
    sequential = args.sequential.resolve()
    openmp = args.openmp.resolve()
    cases = [
        ("single", 1, 1, [255], 5, 1.0, bytes([255])),
        ("constant", 3, 2, [73] * 6, 7, 1.4, bytes([73] * 6)),
        ("left_edge", 2, 1, [0, 255], 3, 1.0, bytes([70, 185])),
        ("top_edge", 1, 2, [255, 0], 3, 1.0, bytes([185, 70])),
        ("identity", 3, 2, [0, 0.5, 73, 92, 254.5, 255], 1, 0.7, bytes([0, 1, 73, 92, 255, 255])),
        ("corner_impulse", 4, 3, [255] + [0] * 11, 5, 1.0, None),
        ("rectangle", 3, 2, [15, 22, 40, 115, 199, 251], 7, 2.0, None),
    ]
    with TemporaryDirectory(prefix="gaussian-tests-") as directory:
        folder = Path(directory)
        default_pixels = [0, 255, 45, 210, 75, 130]
        default_source = folder / "default.bin"
        default_source.write_bytes(struct.pack("<6f", *default_pixels))
        default_pgm = b"P5\n3 2\n255\n" + reference(3, 2, default_pixels, 19, 3)
        for binary, name in ((sequential, "seq"), (openmp, "omp")):
            output = folder / f"default_{name}.pgm"
            run(binary, default_source, 3, 2, output)
            expect(output.read_bytes() == default_pgm, f"{name}: default kernel 19, sigma 3")
        print("PASS default kernel 19, sigma 3")

        for name, width, height, pixels, size, sigma, expected in cases:
            if expected is None:
                expected = reference(width, height, pixels, size, sigma)
            source = folder / f"{name}.bin"
            source.write_bytes(struct.pack(f"<{len(pixels)}f", *pixels))
            header = f"P5\n{width} {height}\n255\n".encode("ascii")
            seq_out = folder / f"{name}_seq.pgm"
            run(sequential, source, width, height, seq_out, size, sigma)
            expect(seq_out.read_bytes() == header + expected, f"{name}: Sequential pixels")
            for threads in args.threads:
                omp_out = folder / f"{name}_omp{threads}.pgm"
                run(openmp, source, width, height, omp_out, size, sigma, threads)
                expect(omp_out.read_bytes() == header + expected, f"{name}: OpenMP {threads} threads")
            print(f"PASS {name}")

        source = folder / "single.bin"
        original = source.read_bytes()
        unused = folder / "unused.pgm"
        run(sequential, source, 1, 1, source, success=False)
        expect(source.read_bytes() == original, "Input must survive output path collision")
        run(sequential, source, 2, 1, unused, success=False)
        run(sequential, folder / "missing.bin", 1, 1, unused, success=False)
        run(sequential, source, 1, 1, unused, 4, success=False)
        run(sequential, source, 1, 1, unused, 5, 0, success=False)
        run(openmp, source, 1, 1, unused, 5, 1, 3, success=False)
        source = folder / "invalid.bin"
        source.write_bytes(struct.pack("<f", float("nan")))
        run(sequential, source, 1, 1, unused, success=False)
        expect(not unused.exists(), "Invalid requests must not create output")
        print("PASS invalid inputs and existing output")


if __name__ == "__main__":
    main()
