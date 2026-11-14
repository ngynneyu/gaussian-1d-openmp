"""Summarize the per-image medians from a complete Gaussian benchmark."""

import argparse
import csv
from pathlib import Path
from statistics import median

from plot_results import load_groups


def summarize(folder):
    config, groups, threads = load_groups(folder)
    fields = (
        "algorithm", "width", "height", "kernel_size", "sigma", "implementation",
        "threads", "images", "median_execution_time_ms", "median_speedup",
        "median_efficiency_percent", "max_abs_error", "max_rmse_over_images",
    )
    with (folder / "summary.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for size, images in groups.items():
            for mode, count in [("seq", 1), *(("omp", n) for n in threads)]:
                rows = [image[mode, count] for image in images]
                result = {
                    "algorithm": config["algorithm"],
                    "width": size,
                    "height": size,
                    "kernel_size": config["kernel_size"],
                    "sigma": config["sigma"],
                    "implementation": mode,
                    "threads": count,
                    "images": len(rows),
                    "median_execution_time_ms": median(float(row["median_ms"]) for row in rows),
                }
                if mode == "omp":
                    result.update(
                        median_speedup=median(float(row["speedup"]) for row in rows),
                        median_efficiency_percent=median(float(row["efficiency_pct"]) for row in rows),
                        max_abs_error=max(float(row["max_abs_error"]) for row in rows),
                        max_rmse_over_images=max(float(row["rmse"]) for row in rows),
                    )
                writer.writerow(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    summarize(parser.parse_args().results)
