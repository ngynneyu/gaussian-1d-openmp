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
MARKERS = ("o", "s", "^")

METRICS = {
    "time": (
        "Thời gian xử lý",
        "Thời gian (ms)",
        "omp_ms"
    ),
    "speedup": (
        "Hệ số tăng tốc",
        "Hệ số tăng tốc (lần)",
        "speedup"
    ),
    "efficiency": (
        "Hiệu suất song song",
        "Hiệu suất (%)",
        "efficiency_pct"
    ),
}


def read_run(directory):
    config = json.loads(
        (directory / "run_config.json").read_text(
            encoding="utf-8"
        )
    )

    threads = sorted(config["threads"])

    if (
        not threads
        or threads[0] < 1
        or len(set(threads)) != len(threads)
    ):
        raise ValueError(
            f"{directory}: invalid OpenMP thread counts"
        )

    with (directory / "summary.csv").open(
        encoding="utf-8-sig", newline=""
    ) as file:

        records = {}

        for row in csv.DictReader(file):
            key = (
                row["algorithm"],
                int(row["size"]),
                int(row["threads"])
            )

            if key in records:
                raise ValueError(
                    f"{directory}: duplicate summary row {key}"
                )

            metrics = {
                name: float(row[name])
                for name in (
                    "seq_ms",
                    "omp_ms",
                    "speedup",
                    "efficiency_pct"
                )
            }

            if any(
                not isfinite(value) or value <= 0
                for value in metrics.values()
            ):
                raise ValueError(
                    f"{directory}: invalid metric in {key}"
                )

            records[key] = metrics

    expected = {
        (algorithm, size, n)
        for algorithm in ALGORITHMS
        for size in SIZES
        for n in threads
    }

    if records.keys() != expected:
        raise ValueError(
            f"{directory}: summary rows do not match "
            "configured threads and datasets"
        )

    platform = config["platform"]

    label = (
        "Windows" if platform.startswith("Windows")
        else "WSL2" if "WSL2" in platform
        else platform.split("-", 1)[0]
    )

    return directory.name, label, threads, records


def series_values(records, algorithm, size, threads, metric):
    values = [
        records[algorithm, size, n][metric]
        for n in threads
    ]

    # The sequential baseline is measured once per image.
    if metric == "omp_ms":
        values.insert(
            0,
            records[algorithm, size, threads[0]]["seq_ms"]
        )

    return values


def plot_runs(runs, output, dpi, ideal_speedup=False):
    import matplotlib
    matplotlib.use("Agg")

    import matplotlib.pyplot as plt

    from matplotlib.ticker import (
        LogLocator,
        FuncFormatter,
        NullFormatter,
        MaxNLocator
    )

    plt.rcParams.update({
        "font.family": "DejaVu Sans",
        "font.size": 12,
        "axes.labelsize": 13,
        "xtick.labelsize": 11,
        "ytick.labelsize": 11,
        "legend.fontsize": 11,
        "axes.linewidth": 0.9,
        "savefig.facecolor": "white"
    })

    for algorithm in ALGORITHMS:

        for slug, (_, y_label, field) in METRICS.items():

            # Use common vertical limits when comparing runs.
            values = [
                value
                for _, _, threads, records in runs
                for size in SIZES
                for value in series_values(
                    records,
                    algorithm,
                    size,
                    threads,
                    field
                )
            ]

            low = min(values)
            peak = max(values)

            if slug == "speedup" and ideal_speedup:
                peak = max(
                    peak,
                    max(
                        threads[-1]
                        for _, _, threads, _ in runs
                    )
                )

            elif slug == "efficiency":
                peak = max(peak, 100)

            for name, _platform, threads, records in runs:

                is_time = slug == "time"

                x = [0, *threads] if is_time else threads

                fig, ax = plt.subplots(
                    figsize=(10.5, 5.8),
                    layout="constrained"
                )

                # Draw experimental results.
                for size, color, marker in zip(
                    SIZES, COLORS, MARKERS
                ):
                    y = series_values(
                        records,
                        algorithm,
                        size,
                        threads,
                        field
                    )

                    ax.plot(
                        x,
                        y,
                        label=f"{size} × {size}",
                        color=color,
                        marker=marker,
                        linewidth=2.0,
                        markersize=5.5
                    )

                # Axis labels.
                ax.set_xlabel(
                    "Cấu hình thực thi"
                    if is_time
                    else "Số luồng OpenMP",
                    labelpad=8
                )

                ax.set_ylabel(y_label, labelpad=10)

                # =====================================
                # 1. Execution time
                # =====================================
                if slug == "time":

                    ax.set_xticks(
                        x,
                        ["Seq", *map(str, threads)]
                    )

                    ax.set_yscale("log")

                    ax.set_ylim(
                        low * 0.8,
                        peak * 1.2
                    )

                    # Display 1, 2, 5, 10, 20, 50, 100...
                    # instead of scientific notation.
                    ax.yaxis.set_major_locator(
                        LogLocator(
                            base=10,
                            subs=(1.0, 2.0, 5.0),
                            numticks=20
                        )
                    )

                    ax.yaxis.set_major_formatter(
                        FuncFormatter(
                            lambda value, _: f"{value:g}"
                        )
                    )

                    ax.yaxis.set_minor_formatter(
                        NullFormatter()
                    )

                    ax.legend(
                        loc="upper right",
                        title="Kích thước ảnh",
                        frameon=True,
                        facecolor="white",
                        edgecolor="#d1d5db",
                        framealpha=0.95
                    )

                # =====================================
                # 2. Speedup
                # =====================================
                elif slug == "speedup":

                    ax.set_xticks(threads)

                    ax.set_ylim(
                        0,
                        peak * 1.15
                    )

                    ax.yaxis.set_major_locator(
                        MaxNLocator(
                            nbins=7,
                            integer=True
                        )
                    )

                    # Optional ideal speedup reference.
                    if ideal_speedup:
                        ax.plot(
                            threads,
                            threads,
                            linestyle="--",
                            linewidth=1.3,
                            color="#71717a",
                            label="Tăng tốc lý tưởng",
                            zorder=1
                        )

                    ax.legend(
                        loc="upper left",
                        title="Kích thước ảnh",
                        frameon=True,
                        facecolor="white",
                        edgecolor="#d1d5db",
                        framealpha=0.95
                    )

                # =====================================
                # 3. Parallel efficiency
                # =====================================
                elif slug == "efficiency":

                    ax.set_xticks(threads)

                    ax.set_ylim(
                        0,
                        max(110, peak * 1.05)
                    )

                    ax.set_yticks(
                        [0, 20, 40, 60, 80, 100]
                    )

                    # Ideal efficiency.
                    ax.axhline(
                        y=100,
                        linestyle="--",
                        linewidth=1.2,
                        color="#71717a",
                        label="Hiệu suất lý tưởng",
                        zorder=1
                    )

                    ax.legend(
                        loc="lower left",
                        frameon=True,
                        facecolor="white",
                        edgecolor="#d1d5db",
                        framealpha=0.95
                    )

                # Common chart formatting.
                ax.grid(
                    True,
                    which="major",
                    linestyle="--",
                    linewidth=0.6,
                    color="#d7dce4",
                    alpha=0.8
                )

                ax.set_axisbelow(True)

                ax.spines["top"].set_visible(False)
                ax.spines["right"].set_visible(False)

                ax.margins(x=0.04)

                # Save chart.
                dest = output / name

                dest.mkdir(
                    parents=True,
                    exist_ok=True
                )

                fig.savefig(
                    dest / f"{algorithm.lower()}_{slug}.png",
                    dpi=dpi,
                    facecolor="white",
                    bbox_inches="tight",
                    pad_inches=0.12
                )

                plt.close(fig)

    return (
        len(runs)
        * len(ALGORITHMS)
        * len(METRICS)
    )


def main():

    parser = argparse.ArgumentParser(
        description=__doc__
    )

    parser.add_argument(
        "runs",
        type=Path,
        nargs="+",
        help=(
            "directories containing summary.csv "
            "and run_config.json"
        )
    )

    parser.add_argument(
        "--out",
        type=Path,
        required=True,
        help="parent directory for generated chart sets"
    )

    parser.add_argument(
        "--dpi",
        type=int,
        default=300,
        help="PNG resolution, default 300"
    )

    parser.add_argument(
        "--ideal-speedup",
        action="store_true",
        help="display the ideal speedup reference line"
    )

    args = parser.parse_args()

    if args.dpi <= 0:
        parser.error("--dpi must be positive")

    try:
        runs = [
            read_run(path)
            for path in args.runs
        ]

    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))

    if len({run[0] for run in runs}) != len(runs):
        parser.error(
            "run directory names must be distinct"
        )

    try:
        count = plot_runs(
            runs,
            args.out,
            args.dpi,
            ideal_speedup=args.ideal_speedup
        )

    except ModuleNotFoundError as exc:

        if exc.name != "matplotlib":
            raise

        parser.error(
            "Matplotlib is required; install it with: "
            "python -m pip install matplotlib"
        )

    print(
        f"{count} PNG charts saved to "
        f"{args.out.resolve()}"
    )


if __name__ == "__main__":
    main()
