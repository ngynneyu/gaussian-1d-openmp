#!/usr/bin/env python3
"""Calculate per-image statistics and summarize the 20 images at each size."""

import argparse
import csv
import json
from math import isfinite
from pathlib import Path
from statistics import median, quantiles

SIZES = (512, 1024, 2048)
ALGORITHMS = ("Sobel", "Gaussian", "Laplacian")


def write_rows(path, rows):
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def summarize(directory):
    config_path = directory / "run_config.json"
    recorded_threads = (json.loads(config_path.read_text(encoding="utf-8"))["threads"]
                        if config_path.is_file() else None)
    timings = {}
    seq_samples = {}
    with (directory / "timings.csv").open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source)
        for row in reader:
            name, size, threads = row["algorithm"], int(row["size"]), int(row["threads"])
            image = row["image"]
            group = timings.setdefault((name, size, threads), {})
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

    threads_used = recorded_threads if recorded_threads is not None else sorted({key[2] for key in timings})
    if not threads_used or min(threads_used) < 1 or len(set(threads_used)) != len(threads_used):
        raise ValueError("Invalid OpenMP thread counts")
    expected_groups = {(name, size, threads) for name in ALGORITHMS
                       for size in SIZES for threads in threads_used}
    if timings.keys() != expected_groups:
        raise ValueError("Timing rows do not match the expected filters, sizes and thread counts")

    details, summary = [], []
    expected = {f"image_{index:02}" for index in range(1, 21)}
    for name, size, threads in sorted(expected_groups, key=lambda key: (ALGORITHMS.index(key[0]), *key[1:])):
        group = timings[name, size, threads]
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
        seq_q1, _, seq_q3 = quantiles(seq, n=4, method="inclusive")
        omp_q1, _, omp_q3 = quantiles(omp, n=4, method="inclusive")
        speedup = seq_ms / omp_ms
        summary.append(dict(algorithm=name, size=size, threads=threads, images=len(group),
                            seq_ms=seq_ms, omp_ms=omp_ms, speedup=speedup,
                            efficiency_pct=100 * speedup / threads,
                            max_abs_error=max(max_error), max_rmse=max(rmse),
                            seq_q1_ms=seq_q1, seq_q3_ms=seq_q3,
                            omp_q1_ms=omp_q1, omp_q3_ms=omp_q3))
    write_rows(directory / "metrics_per_image.csv", details)
    write_rows(directory / "summary.csv", summary)
    print(f"{len(details)} comparisons, {len(summary)} summary rows")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    summarize(parser.parse_args().directory)
