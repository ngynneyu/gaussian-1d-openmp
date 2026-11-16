#!/usr/bin/env python3
"""Draw benchmark charts from one or more completed runs."""

import argparse
import csv
import json
from math import isfinite
from pathlib import Path

if __package__:
    from .summarize import ALGORITHMS, SIZES
else:
    from summarize import ALGORITHMS, SIZES


COLORS = ("#1f77b4", "#d62728", "#2ca02c")
METRICS = {
    "time": ("Thời gian xử lý", "Thời gian (ms)", "omp_ms"),
    "speedup": ("Speedup", "Speedup (×)", "speedup"),
    "efficiency": ("Hiệu suất song song", "Hiệu suất (%)", "efficiency_pct"),
}


def read_run(directory):
    config = json.loads((directory / "run_config.json").read_text(encoding="utf-8"))
    threads = sorted(config["threads"])
    if not threads or threads[0] < 2 or len(set(threads)) != len(threads):
        raise ValueError(f"{directory}: invalid OpenMP thread counts")
    with (directory / "summary.csv").open(encoding="utf-8-sig", newline="") as file:
        records = {}
        for row in csv.DictReader(file):
            key = (row["algorithm"], int(row["size"]), int(row["threads"]))
            if key in records:
                raise ValueError(f"{directory}: duplicate summary row {key}")
            metrics = {name: float(row[name]) for name in (
                "seq_ms", "omp_ms", "speedup", "efficiency_pct"
            )}
            if any(not isfinite(value) or value <= 0 for value in metrics.values()):
                raise ValueError(f"{directory}: invalid metric in {key}")
            records[key] = metrics
    expected = {(algorithm, size, n) for algorithm in ALGORITHMS for size in SIZES for n in threads}
    if records.keys() != expected:
        raise ValueError(f"{directory}: summary rows do not match configured threads and datasets")
    platform = config["platform"]
    label = ("Windows" if platform.startswith("Windows") else
             "WSL2" if "WSL2" in platform else platform.split("-", 1)[0])
    return directory.name, label, threads, records


def series_values(records, algorithm, size, threads, metric):
    values = [records[algorithm, size, n][metric] for n in threads]
    # The sequential baseline is measured once per image and reused for every OpenMP configuration.
    if metric == "omp_ms":
        values.insert(0, records[algorithm, size, threads[0]]["seq_ms"])
    return values


def plot_runs(runs, output, dpi):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    for algorithm in ALGORITHMS:
        for slug, (title, y_label, field) in METRICS.items():
            # Use a common vertical scale when comparing runs in the same invocation.
            peak = max(value for _, _, threads, records in runs for size in SIZES
                       for value in series_values(records, algorithm, size, threads, field))
            for name, platform, threads, records in runs:
                is_time = slug == "time"
                x = [1, *threads] if is_time else threads
                fig, ax = plt.subplots(figsize=(10.5, 5.8), layout="constrained")
                for size, color in zip(SIZES, COLORS):
                    ax.plot(x, series_values(records, algorithm, size, threads, field),
                            label=f"{size} × {size}", color=color, marker="o",
                            linewidth=2.4, markersize=6.5)
                ax.set_title(f"{title} theo số luồng: {algorithm}", fontsize=15, weight="bold", pad=16)
                ax.set_xlabel("Số luồng (1 = bản tuần tự)" if is_time else "Số luồng")
                ax.set_ylabel(y_label)
                ax.set_xticks(x)
                ax.set_ylim(0, peak * 1.10)
                ax.grid(True, linestyle="--", linewidth=0.65, color="#d7dce4")
                ax.set_axisbelow(True)
                ax.spines[["top", "right"]].set_visible(False)
                ax.legend(title="Kích thước ảnh (px)", loc="best", facecolor="white", framealpha=0.95)
                fig.supxlabel(platform, fontsize=9, color="#505a69")
                dest = output / name
                dest.mkdir(parents=True, exist_ok=True)
                fig.savefig(dest / f"{algorithm.lower()}_{slug}.png", dpi=dpi, facecolor="white")
                plt.close(fig)
    return len(runs) * len(ALGORITHMS) * len(METRICS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", type=Path, nargs="+", help="directories containing summary.csv and run_config.json")
    parser.add_argument("--out", type=Path, required=True, help="parent directory for the generated chart sets")
    parser.add_argument("--dpi", type=int, default=160, help="PNG resolution, default 160")
    args = parser.parse_args()
    if args.dpi <= 0:
        parser.error("--dpi must be positive")
    try:
        runs = [read_run(path) for path in args.runs]
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    if len({run[0] for run in runs}) != len(runs):
        parser.error("run directory names must be distinct")
    try:
        count = plot_runs(runs, args.out, args.dpi)
    except ModuleNotFoundError as exc:
        if exc.name != "matplotlib":
            raise
        parser.error("Matplotlib is required; install it with: python -m pip install matplotlib")
    print(f"{count} PNG charts saved to {args.out.resolve()}")


if __name__ == "__main__":
    main()
