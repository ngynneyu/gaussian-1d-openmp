"""Compare sequential and 16-thread row-parallel filters on 100 images."""

import argparse
import csv
from datetime import datetime
import hashlib
from itertools import chain, product
import json
import math
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parent
SRC = PROJECT / "src"
ORIGINAL = PROJECT / "data" / "raw"
ADDED = Path(os.environ.get("IMAGE_FILTER_SCALING_EXTRA_DATA", ROOT / "data" / "images_2k")).expanduser()
FILTERS = ("Gaussian", "Sobel", "Laplacian")
MODES = (("seq", 0), ("intra", 16))
MIN_SAMPLE_SECONDS = 0.020
MIN_SAMPLE_TICKS = 10
FLAGS = ("-std=gnu11", "-O3", "-fopenmp", "-ffp-contract=off", "-Wall", "-Wextra")
EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif", ".tiff"}
OMP_ENV = {"OMP_DYNAMIC": "FALSE", "OMP_THREAD_LIMIT": "16", "OMP_WAIT_POLICY": "PASSIVE",
           "OMP_PROC_BIND": "FALSE", "GOMP_SPINCOUNT": "0"}
UNITS = [ROOT / "benchmark.c", SRC / "gaussian" / "gaussian.c", SRC / "gaussian" / "gaussian_omp.c",
         SRC / "sobel" / "sobel_seq.c", SRC / "sobel" / "sobel_omp.c",
         SRC / "laplacian" / "laplacian_adapter.c"]
SOURCE_FILES = [*UNITS, SRC / "gaussian" / "gaussian.h", SRC / "sobel" / "sobel.h",
                SRC / "laplacian" / "laplacian_adapter.h", SRC / "laplacian" / "laplacian_benchmark.c"]


def sha256(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def image_paths(folder):
    return sorted(path for path in folder.iterdir()
                  if path.is_file() and path.suffix.lower() in EXTENSIONS)


def select_sources(count):
    originals = image_paths(ORIGINAL)
    if len(originals) != 20:
        raise ValueError(f"Expected 20 original images in {ORIGINAL}.")
    if len({sha256(path) for path in originals}) != 20:
        raise ValueError("The 20 original images must be distinct files.")
    extra = image_paths(ADDED) if count > 20 else []
    candidates = chain((("original", path) for path in originals),
                       (("added", path) for path in extra))
    seen, selected = set(), []
    for group, path in candidates:
        digest = sha256(path)
        if digest in seen:
            continue
        seen.add(digest)
        selected.append((group, path, digest))
        if len(selected) == count:
            break
    if len(selected) != count:
        raise ValueError(f"Found only {len(selected)} distinct files for {count} images.")
    if count == 100 and sum(group == "original" for group, _, _ in selected) != 20:
        raise ValueError("The 100-image workload requires 20 distinct original files.")
    return selected


def prepare_dataset(side, count):
    dataset = ROOT / "data" / f"{side}_{count}"
    binary = dataset / "images.f32"
    expected_bytes = count * side * side * 4
    original_available = ORIGINAL.is_dir() and len(image_paths(ORIGINAL)) == 20
    extra_available = count <= 20 or (
        ADDED.is_dir() and len(image_paths(ADDED)) >= count - 20
    )
    selected = select_sources(count) if original_available and extra_available else None
    if binary.is_file():
        config = json.loads((dataset / "dataset.json").read_text(encoding="utf-8"))
        if (config["images"], config["side"], binary.stat().st_size) != (count, side, expected_bytes):
            raise ValueError(f"Invalid prepared data in {dataset}.")
        if sha256(binary) != config["sha256"]:
            raise ValueError(f"Prepared data did not match its checksum in {dataset}.")
        cached_manifest = [(row["image_index"], row["source_group"], row["original_file"], row["source_sha256"])
                           for row in csv_rows(dataset / "manifest.csv")]
        if len(cached_manifest) != count:
            raise ValueError(f"Invalid source manifest in {dataset}.")
        if selected is not None:
            expected_manifest = [(str(index), group, path.name, digest)
                                 for index, (group, path, digest) in enumerate(selected, 1)]
        else:
            expected_manifest = None
        if expected_manifest is not None and cached_manifest != expected_manifest:
            raise ValueError(f"Source selection has changed; move the old prepared dataset {dataset} aside and rerun.")
        print(f"Using prepared data in {dataset}", flush=True)
        return dataset, config

    if selected is None:
        raise FileNotFoundError(
            f"Prepared data is missing in {dataset}. To rebuild it, provide 20 images in {ORIGINAL} "
            f"and 80 additional images in {ADDED} (or set IMAGE_FILTER_SCALING_EXTRA_DATA).")

    saved_config = None
    saved_manifest_sha256 = None
    if dataset.exists():
        config_path = dataset / "dataset.json"
        manifest_path = dataset / "manifest.csv"
        if not config_path.is_file() or not manifest_path.is_file():
            raise ValueError(f"Cannot rebuild {dataset}: dataset.json or manifest.csv is missing.")
        saved_config = json.loads(config_path.read_text(encoding="utf-8"))
        saved_manifest_sha256 = sha256(manifest_path)

    dataset.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    with tempfile.TemporaryDirectory(dir=dataset.parent, prefix="prepare_") as staging:
        stage = Path(staging)
        with (stage / binary.name).open("wb") as pixels, (stage / "manifest.csv").open(
                "w", encoding="utf-8", newline="") as manifest:
            writer = csv.writer(manifest)
            writer.writerow(("image_index", "source_group", "original_file", "source_sha256"))
            for index, (group, path, source_digest) in enumerate(selected, 1):
                with Image.open(path) as image:
                    gray = ImageOps.exif_transpose(image).convert("L")
                width, height = gray.size
                length = min(width, height)
                if length < side:
                    raise ValueError(f"Image {path} is smaller than {side} pixels on one side.")
                left, top = (width - length) // 2, (height - length) // 2
                square = gray.crop((left, top, left + length, top + length))
                resized = square.resize((side, side), Image.Resampling.LANCZOS)
                content = np.asarray(resized, dtype="<f4").tobytes()
                pixels.write(content)
                digest.update(content)
                writer.writerow((index, group, path.name, source_digest))
                if index == count or index % 20 == 0:
                    print(f"Prepared {index}/{count} images", flush=True)
        config = {"side": side, "images": count, "format": "row-major float32 little-endian",
                  "sha256": digest.hexdigest()}
        (stage / "dataset.json").write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        if saved_config is not None:
            if config != saved_config:
                raise ValueError("Rebuilt dataset does not match the official dataset.json.")
            if sha256(stage / "manifest.csv") != saved_manifest_sha256:
                raise ValueError("Rebuilt manifest does not match the official manifest.csv.")
            os.replace(stage / binary.name, binary)
        else:
            stage.rename(dataset)
    return dataset, config


def compiler_path(requested):
    if requested:
        return shutil.which(requested) or str(Path(requested).resolve(strict=True))
    compiler = shutil.which("gcc")
    if compiler:
        return compiler
    raise OSError(
        "GCC with OpenMP was not found. Add GCC to PATH or provide its path through --cc."
    )


def csv_rows(file):
    with file.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source))


def build(cc, folder, flags=FLAGS):
    exe = folder / ("filter_batch.exe" if os.name == "nt" else "filter_batch")
    subprocess.run([cc, *flags, "-I", str(SRC), *(str(unit) for unit in UNITS),
                    "-o", str(exe), "-lm"], check=True)
    return exe


def export_images(out, dataset, config, exe):
    target = out / "images"
    if target.exists():
        raise FileExistsError(f"Images already exist in {target}.")
    side, count = config["dataset"]["side"], config["dataset"]["images"]
    manifest = csv_rows(out / "manifest.csv")
    if [int(row["image_index"]) for row in manifest] != list(range(1, count + 1)):
        raise ValueError("The image manifest does not match the exported batch.")
    names = [f"{index:03d}_{Path(row['original_file']).stem}.png"
             for index, row in enumerate(manifest, 1)]

    with tempfile.TemporaryDirectory(dir=out, prefix="export_") as temporary:
        staged = Path(temporary) / "images"
        args = [str(exe), str(dataset / "images.f32"), str(side), "--export",
                config["filter_selection"], str(count)]
        with subprocess.Popen(args, stdout=subprocess.PIPE) as process:
            for algorithm in config["algorithms"]:
                versions = [staged / algorithm / version for version in
                            ("seq", "intra16")]
                for folder in versions:
                    folder.mkdir(parents=True)
                frame_bytes = side * side * (1 if algorithm == "Gaussian" else 4)
                for name in names:
                    pixels = process.stdout.read(frame_bytes)
                    if len(pixels) != frame_bytes:
                        raise RuntimeError(f"Incomplete output for {algorithm}/{name}.")
                    if algorithm == "Gaussian":
                        image = Image.frombytes("L", (side, side), pixels)
                    else:
                        values = np.frombuffer(pixels, dtype="<f4").reshape(side, side)
                        # Same display mapping as the group benchmark for edge filters.
                        image = Image.fromarray(np.clip(np.abs(values) / 4, 0, 255).astype(np.uint8))
                    with image:
                        image.save(versions[0] / name, compress_level=3)
                    # The OpenMP output was verified against the sequential pixels.
                    for folder in versions[1:]:
                        shutil.copyfile(versions[0] / name, folder / name)
                print(f"Exported {count} images for each version of {algorithm}", flush=True)
            if process.stdout.read(1) or process.wait():
                raise RuntimeError("Filter output stream did not finish correctly.")
        staged.rename(target)
    print(f"Images: {target}")


def export_existing(out, cc):
    config = json.loads((out / "run_config.json").read_text(encoding="utf-8"))
    if config["status"] != "complete":
        raise ValueError("PNG export requires a completed run with matching outputs.")
    if (out / "images").exists():
        raise FileExistsError(f"Images already exist in {out / 'images'}.")
    if config.get("schema_version") != 2 or config.get("benchmark_version") != 5:
        raise ValueError("Use the original program to export a legacy batch run.")
    for unit in SOURCE_FILES:
        if sha256(unit) != config["source_sha256"][unit.name]:
            raise ValueError(f"The filter source changed since this run: {unit}")
    side, count = config["dataset"]["side"], config["dataset"]["images"]
    dataset, data_config = prepare_dataset(side, count)
    if data_config != config["dataset"] or sha256(dataset / "manifest.csv") != config["manifest_sha256"]:
        raise ValueError("The prepared images are different from the dataset used in this run.")
    with tempfile.TemporaryDirectory(dir=out, prefix="build_export_") as temporary:
        exe = build(cc, Path(temporary), config["flags"])
        export_images(out, dataset, config, exe)


def summarize(out, counts, repeats, filters, minimum):
    timings = csv_rows(out / "timings.csv")
    elapsed = {}
    for row in timings:
        key = (row["algorithm"], int(row["image_count"]), row["strategy"],
               int(row["threads"]), int(row["repeat"]))
        seconds = float(row["total_seconds"])
        raw = float(row["elapsed_seconds"])
        batches = int(row["batches_per_sample"])
        if key in elapsed or batches < 1 or not all(math.isfinite(x) and x > 0 for x in (raw, seconds)):
            raise ValueError("Duplicate or invalid timing sample.")
        if raw + 1e-12 < minimum or not math.isclose(raw / batches, seconds, rel_tol=1e-12):
            raise ValueError("Timing sample does not match the adaptive batch rule.")
        elapsed[key] = seconds
    expected = {(f, n, mode, p, r) for f, n, (mode, p), r in
                product(filters, counts, MODES, range(1, repeats + 1))}
    if elapsed.keys() != expected:
        raise ValueError("Missing or unexpected batch measurements.")
    rows = csv_rows(out / "errors_per_image.csv")
    errors = {(row["algorithm"], row["strategy"], int(row["threads"]), int(row["image_index"])):
              (float(row["max_abs_error"]), float(row["rmse"])) for row in rows}
    expected_errors = {(f, mode, p, i) for f, (mode, p), i in
                       product(filters, MODES[1:], range(1, counts[-1] + 1))}
    if len(errors) != len(rows) or errors.keys() != expected_errors:
        raise ValueError("Missing or duplicate image comparisons.")
    if any(error < 0 or not math.isfinite(error) for pair in errors.values() for error in pair):
        raise ValueError("Invalid errors in errors_per_image.csv.")

    def stats(samples):
        med = statistics.median(samples)
        q1, _, q3 = statistics.quantiles(samples, n=4, method="inclusive")
        return med, q1, q3

    with (out / "summary.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(("algorithm", "image_count", "strategy", "threads", "median_ms",
                         "q1_ms", "q3_ms", "ms_per_image", "speedup", "efficiency_pct",
                         "images_per_second", "max_abs_error", "max_rmse"))
        for algorithm, count in product(filters, counts):
            measures = {mode: stats([elapsed[algorithm, count, *mode, r]
                                     for r in range(1, repeats + 1)]) for mode in MODES}
            baseline = measures[("seq", 0)][0]
            for (strategy, threads), (seconds, q1, q3) in measures.items():
                speedup = baseline / seconds
                image_errors = ([errors[algorithm, strategy, threads, i] for i in range(1, count + 1)]
                                if threads else [(0.0, 0.0)])
                maxae, rmse = map(max, zip(*image_errors))
                writer.writerow((algorithm, count, strategy, threads, f"{seconds * 1000:.6f}",
                                 f"{q1 * 1000:.6f}", f"{q3 * 1000:.6f}", f"{seconds * 1000 / count:.6f}",
                                 f"{speedup:.8f}", f"{100 * speedup / threads:.6f}" if threads else "",
                                 f"{count / seconds:.8f}",
                                 f"{maxae:.12g}", f"{rmse:.12g}"))
    return all(maxae == 0 and rmse == 0 for maxae, rmse in errors.values())


def system_info():
    info = {"platform": platform.platform(), "processor": platform.processor(),
            "logical_processors": os.cpu_count()}
    if os.name == "nt":
        # Optional metadata collection must not prevent a benchmark from running.
        try:
            script = ("$p=Get-CimInstance Win32_Processor; $s=Get-CimInstance Win32_ComputerSystem; "
                      "[pscustomobject]@{cpu=$p.Name; physical_cores=$p.NumberOfCores; "
                      "logical_processors=$p.NumberOfLogicalProcessors; max_clock_mhz=$p.MaxClockSpeed; "
                      "ram_bytes=$s.TotalPhysicalMemory; model=$s.Model} | ConvertTo-Json -Compress")
            result = subprocess.check_output(["powershell", "-NoProfile", "-Command", script],
                                             text=True, timeout=20)
            info["hardware"] = json.loads(result)
            info["power_scheme"] = subprocess.check_output(["powercfg", "/getactivescheme"],
                                                            text=True, errors="replace", timeout=10).strip()
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            info["hardware_metadata_warning"] = str(exc)
    return info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", help="GCC executable")
    parser.add_argument("--smoke", action="store_true", help="4 images of 128x128; 2 rounds")
    parser.add_argument("--filter", choices=("all", *FILTERS), default="all")
    parser.add_argument("--side", type=int, default=2048)
    parser.add_argument("--counts", type=int, nargs="+", default=[100], help="image counts (default: 100 only)")
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--images", action="store_true", help="save verified PNGs after measurements")
    parser.add_argument("--export-images", type=Path, metavar="RUN", help="export PNGs for a completed run using matching source files")
    args = parser.parse_args()
    cc = compiler_path(args.cc)
    if args.export_images:
        if args.smoke or args.images:
            parser.error("--export-images is a separate operation from running the benchmark")
        export_existing(args.export_images.resolve(), cc)
        return
    side, counts, repeats = (128, (4,), 2) if args.smoke else (args.side, tuple(args.counts), args.repeats)
    if not 2 <= side <= 2048 or not 2 <= repeats <= 100 or counts != tuple(sorted(set(counts))) or not 1 <= counts[0] <= counts[-1] <= 100:
        parser.error("Require side 2..2048, repeats 2..100, and strictly increasing counts in 1..100.")
    filters = FILTERS if args.filter == "all" else (args.filter,)
    out = ROOT / "results" / datetime.now().strftime(("smoke_v5_" if args.smoke else "batch_v5_") + "%Y%m%d_%H%M%S_%f")
    out.mkdir(parents=True)
    exe = build(cc, out)
    dataset, dataset_config = prepare_dataset(side, counts[-1])
    shutil.copyfile(dataset / "manifest.csv", out / "manifest.csv")
    env = dict(os.environ, **OMP_ENV)
    for name in ("GOMP_CPU_AFFINITY", "OMP_PLACES"):
        env.pop(name, None)
    config = {"schema_version": 2, "benchmark_version": 5, "started": datetime.now().astimezone().isoformat(timespec="seconds"),
              **system_info(), "compiler": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
              "flags": FLAGS, "source_sha256": {unit.name: sha256(unit) for unit in SOURCE_FILES},
              "algorithms": filters, "filter_selection": args.filter, "gaussian": {"kernel_size": 13, "sigma": 2},
              "image_counts": counts, "modes": [{"strategy": mode, "threads": p} for mode, p in MODES],
              "repeats": repeats, "warmup_batches_per_configuration": 1,
              "timer": "QueryPerformanceCounter" if os.name == "nt" else "omp_get_wtime",
              "timing_sampling": "adaptive_batch", "minimum_sample_seconds_requested": MIN_SAMPLE_SECONDS,
              "minimum_sample_ticks": MIN_SAMPLE_TICKS, "order": "rotate_counts_and_modes_per_round",
              "omp_environment": OMP_ENV, "dataset": dataset_config,
              "manifest_sha256": sha256(out / "manifest.csv"), "status": "running"}
    path = out / "run_config.json"
    path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    print(f"Results: {out}", flush=True)
    try:
        subprocess.run([str(exe), str(dataset / "images.f32"), str(side), str(repeats), args.filter,
                        *(str(count) for count in counts)], cwd=out, env=env, check=True)
        timer = csv_rows(out / "timer_info.csv")[0]
        config.update({key: float(value) for key, value in timer.items()})
        matches = summarize(out, counts, repeats, filters, config["minimum_sample_seconds"])
        config["status"] = "complete" if matches else "mismatch"
    except BaseException:
        config["status"] = "failed"
        raise
    finally:
        path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    if not matches:
        raise SystemExit("An OpenMP result differs from Sequential. See errors_per_image.csv.")
    if args.images:
        export_images(out, dataset, config, exe)
    print(f"Verified all outputs. Results: {out}")


if __name__ == "__main__":
    main()
