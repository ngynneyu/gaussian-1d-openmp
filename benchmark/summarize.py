#!/usr/bin/env python3
"""Calculate per-image statistics and summarize the 20 images at each size."""

import argparse
import csv
import json
from math import isfinite
from pathlib import Path
from statistics import median

if __package__:
    from .run import THREADS
else:
    from run import THREADS

SIZES = (512, 1024, 2048)
ALGORITHMS = ("Sobel", "Gaussian", "Laplacian")


def write_rows(path, rows):
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def summarize(directory):
    config_path = directory / "run_config.json"
    threads_used = THREADS
    if config_path.is_file():
        threads_used = json.loads(config_path.read_text(encoding="utf-8"))["threads"]
    with (directory / "timings.csv").open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        timings = {(name, size, threads): {} for name in ALGORITHMS
                   for size in SIZES for threads in threads_used}
        seq_samples = {}
        for row in reader:
            name, size, threads = row["algorithm"], int(row["size"]), int(row["threads"])
            image = row["image"]
            group = timings[name, size, threads]
            if image in group:
                raise ValueError(f"Duplicate measurement: {name}, {size}, {image}, {threads}")
            seq = tuple(float(row[f"seq_{r}_ms"]) for r in range(1, 11))
            omp = tuple(float(row[f"omp_{r}_ms"]) for r in range(1, 11))
            errors = (float(row["max_abs_error"]), float(row["rmse"]))
            if any(not isfinite(value) or value <= 0 for value in seq + omp):
                raise ValueError(f"Invalid timing: {name}, {size}, {image}, {threads}")
            if any(not isfinite(value) or value < 0 for value in errors):
                raise ValueError(f"Invalid error: {name}, {size}, {image}, {threads}")
            if seq_samples.setdefault((name, size, image), seq) != seq:
                raise ValueError(f"Inconsistent sequential baseline: {name}, {size}, {image}")
            group[image] = (median(seq), median(omp), *errors)

    details, summary = [], []
    expected = {f"image_{index:02}" for index in range(1, 21)}
    for (name, size, threads), group in timings.items():
        if group.keys() != expected:
            raise ValueError(f"Expected all 20 image IDs for {name}, {size}, {threads} threads")
        for image, (seq, omp, max_error, rmse) in sorted(group.items()):
            speedup = seq / omp
            details.append(dict(algorithm=name, size=size, threads=threads, image=image,
                                seq_ms=seq, omp_ms=omp, speedup=speedup,
                                efficiency_pct=100 * speedup / threads,
                                max_abs_error=max_error, rmse=rmse))
        seq, omp, max_error, rmse = zip(*group.values())
        seq_ms, omp_ms = median(seq), median(omp)
        speedup = seq_ms / omp_ms
        summary.append(dict(algorithm=name, size=size, threads=threads, images=len(group),
                            seq_ms=seq_ms, omp_ms=omp_ms, speedup=speedup,
                            efficiency_pct=100 * speedup / threads,
                            max_abs_error=max(max_error), max_rmse=max(rmse)))
    write_rows(directory / "metrics_per_image.csv", details)
    write_rows(directory / "summary.csv", summary)
    print(f"{len(details)} comparisons, {len(summary)} summary rows")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    summarize(parser.parse_args().directory)
