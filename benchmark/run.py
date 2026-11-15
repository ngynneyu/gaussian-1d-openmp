#!/usr/bin/env python3
"""Build and run the same C benchmark for the three image filters (GNU/OpenMP)."""

import argparse
from contextlib import nullcontext
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
from tempfile import TemporaryDirectory
import time

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    "gaussian": ("ngynneyu/parallel-image-filtering-openmp", "2f639af25dd36171978e106c9f467e8a4263101d"),
    "laplacian": ("vietquang8071/ttss", "91e2ed2ac05a56f2dc8bc258f60028242777b977"),
}
GAUSSIAN_UNITS = ("gaussian.c", "gaussian_omp.c", "image_io.c")
FLAGS = ["-std=gnu11", "-O3", "-fopenmp", "-ffp-contract=off", "-Wall", "-Wextra"]
THREADS = (2, 4, 8, 12, 16)
SIZES = (512, 1024, 2048)
IMAGE_IDS = tuple(f"image_{index:02}" for index in range(1, 21))


def run(*args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def build(cc):
    gaussian = Path("src/gaussian")
    laplacian = Path("src/laplacian")
    build_dir = Path(".bench-cache/build")
    build_dir.mkdir(parents=True, exist_ok=True)
    units = [gaussian / name for name in GAUSSIAN_UNITS]
    units += [gaussian / "gaussian_adapter.c", laplacian / "laplacian_adapter.c"]
    units += [Path("src/sobel/sobel_seq.c"), Path("src/sobel/sobel_omp.c"), Path("benchmark/main.c")]
    exe = build_dir / ("image_benchmark.exe" if os.name == "nt" else "image_benchmark")
    run(cc, *FLAGS, "-I", "src", *(str(source) for source in units), "-o", str(exe), "-lm")
    return exe


def cpu_name():
    if os.name == "nt":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            return winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        for line in cpuinfo.read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor()


def dataset_digest(directory):
    digest = hashlib.sha256()
    for size in SIZES:
        for image_id in IMAGE_IDS:
            name = f"{size}/{image_id}.bin"
            file = directory / name
            if file.stat().st_size != size * size * 4:
                raise ValueError(f"Unexpected byte count in {file}")
            digest.update(name.encode("ascii") + b"\0")
            digest.update(file.read_bytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--dataset", type=Path, default=Path("data/processed"))
    parser.add_argument("--out", type=Path, default=Path("results/benchmark"))
    parser.add_argument("--images", action="store_true", help="save all verified outputs as PNG")
    args = parser.parse_args()
    os.chdir(ROOT)
    if args.images:
        try:
            if __package__:
                from .export_png import export_images, image_filenames
            else:
                from export_png import export_images, image_filenames
        except ImportError as error:
            parser.error(
                f'PNG export requires Pillow >= 10.3. Install with pip install "Pillow>=10.3" ({error})'
            )
        filenames = image_filenames(args.dataset, IMAGE_IDS)

    digest = dataset_digest(args.dataset)
    exe = build(args.cc)
    args.out.mkdir(parents=True)
    env = dict(os.environ, OMP_DYNAMIC="FALSE", OMP_WAIT_POLICY="PASSIVE",
               OMP_THREAD_LIMIT=str(max(THREADS)), OMP_PROC_BIND="FALSE", GOMP_SPINCOUNT="0")
    for name in ("GOMP_CPU_AFFINITY", "OMP_PLACES"):
        env.pop(name, None)
    config = {
        "started": datetime.now().astimezone().isoformat(timespec="seconds"),
        "cpu": cpu_name(), "logical_processors": os.cpu_count(), "platform": platform.platform(),
        "c_compiler": run(args.cc, "--version", capture_output=True, text=True).stdout.splitlines()[0],
        "language": "C11", "timer": "QueryPerformanceCounter" if os.name == "nt" else "omp_get_wtime",
        "flags": FLAGS, "threads": THREADS, "warmup": 1, "repeats": 10,
        "environment": {key: env[key] for key in ("OMP_DYNAMIC", "OMP_WAIT_POLICY",
                       "OMP_THREAD_LIMIT", "OMP_PROC_BIND", "GOMP_SPINCOUNT")},
        "dataset_sha256": digest, "sources": PINS,
    }
    config_path = args.out / "run_config.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    command = [str(exe), str(args.dataset), str(args.out)]
    workspace = TemporaryDirectory(dir=args.out, prefix=".png-") if args.images else nullcontext()
    with workspace as temporary_dir:
        if temporary_dir is not None:
            command.append(temporary_dir)
        start = time.monotonic()
        try:
            run(*command, env=env)
        except OSError as error:
            if getattr(error, "winerror", None) != 4551:
                raise
            raise SystemExit(
                f"WinError 4551: Windows Application Control blocked {exe.resolve()}. "
                "See the Windows and WSL2 instructions in README.md."
            ) from error
        config["wall_seconds"] = round(time.monotonic() - start, 2)
        config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        if temporary_dir is not None:
            raw = Path(temporary_dir)
            image_dir = raw / "images"
            print("Exporting PNG files...", flush=True)
            image_count = export_images(raw, image_dir, SIZES, filenames, THREADS)
            image_dir.rename(args.out / "images")
            print(f"{image_count} PNG files saved in {args.out / 'images'}")


if __name__ == "__main__":
    main()
