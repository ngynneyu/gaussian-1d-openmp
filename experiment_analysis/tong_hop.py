"""Validate and aggregate the three official main-experiment runs."""

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd


HERE = Path(__file__).resolve().parent
REPO = HERE.parent
RUN_NAMES = (
    "windows_20261119_152801_1982409",
    "windows_20261119_153808_5914934",
    "windows_20261119_154648_2619652",
)
ALGORITHMS = ("Gaussian", "Sobel", "Laplacian")
SIZES = (512, 1024, 2048)
THREADS = (1, 2, 4, 8, 12, 16)
KEYS = ["algorithm", "size", "image", "threads"]
SUMMARY_KEYS = ["algorithm", "size", "threads"]
SEQ_COLUMNS = [f"seq_{index}_ms" for index in range(1, 11)]
OMP_COLUMNS = [f"omp_{index}_ms" for index in range(1, 11)]
FLOAT_TOLERANCE = 1.1e-6


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_runs():
    summaries, per_image, audit, signatures = [], [], [], []
    expected_keys = {
        (algorithm, size, f"image_{index:02}", threads)
        for algorithm in ALGORITHMS
        for size in SIZES
        for index in range(1, 21)
        for threads in THREADS
    }

    for run_number, run_name in enumerate(RUN_NAMES, start=1):
        folder = REPO / "results" / run_name
        config = json.loads((folder / "run_config.json").read_text(encoding="utf-8"))
        require(config.get("timer") == "QueryPerformanceCounter",
                f"{run_name}: expected the official Windows QPC timer")
        require(tuple(config.get("threads", ())) == THREADS,
                f"{run_name}: unexpected OpenMP thread counts")
        if "status" in config:
            require(config["status"] == "complete", f"{run_name}: run is not complete")

        signature = {key: value for key, value in config.items()
                     if key not in {"started", "wall_seconds", "status"}}
        signatures.append(signature)

        timings = pd.read_csv(folder / "timings.csv")
        images = pd.read_csv(folder / "metrics_per_image.csv")
        summary = pd.read_csv(folder / "summary.csv")
        require(len(timings) == 1080, f"{run_name}: expected 1080 timing rows")
        require(len(images) == 1080, f"{run_name}: expected 1080 per-image comparisons")
        require(len(summary) == 54, f"{run_name}: expected 54 summary rows")
        require(not timings.duplicated(KEYS).any(), f"{run_name}: duplicate timing keys")
        require(not images.duplicated(KEYS).any(), f"{run_name}: duplicate per-image keys")
        require(not summary.duplicated(SUMMARY_KEYS).any(), f"{run_name}: duplicate summary keys")
        require(set(map(tuple, timings[KEYS].itertuples(index=False, name=None))) == expected_keys,
                f"{run_name}: timing keys do not cover all filters, sizes, images, and threads")
        require(set(map(tuple, images[KEYS].itertuples(index=False, name=None))) == expected_keys,
                f"{run_name}: per-image keys do not match timing keys")
        require(set(map(tuple, summary[SUMMARY_KEYS].itertuples(index=False, name=None))) == {
            (algorithm, size, threads)
            for algorithm in ALGORITHMS for size in SIZES for threads in THREADS
        }, f"{run_name}: summary keys are incomplete")

        sample_columns = [*SEQ_COLUMNS, *OMP_COLUMNS]
        require(set(sample_columns) <= set(timings.columns), f"{run_name}: missing raw time samples")
        samples = timings[sample_columns].to_numpy(dtype=float)
        require(np.isfinite(samples).all() and (samples > 0).all(),
                f"{run_name}: timing samples must be finite and positive")
        require(np.isfinite(timings[["max_abs_error", "rmse"]].to_numpy(dtype=float)).all(),
                f"{run_name}: error metrics must be finite")
        require((timings[["max_abs_error", "rmse"]].to_numpy(dtype=float) >= 0).all(),
                f"{run_name}: error metrics must be non-negative")

        # Sequential samples are repeated in the CSV for each OpenMP thread count.
        seq_variation = timings.groupby(["algorithm", "size", "image"])[SEQ_COLUMNS].nunique()
        require((seq_variation == 1).all().all(),
                f"{run_name}: repeated sequential samples differ between thread rows")

        recomputed = timings[KEYS + ["max_abs_error", "rmse"]].copy()
        recomputed["seq_ms"] = timings[SEQ_COLUMNS].median(axis=1)
        recomputed["omp_ms"] = timings[OMP_COLUMNS].median(axis=1)
        image_check = recomputed.merge(images, on=KEYS, suffixes=("_raw", "_csv"),
                                       validate="one_to_one", indicator=True)
        require((image_check["_merge"] == "both").all(),
                f"{run_name}: timing/per-image records do not match")
        for column in ("seq_ms", "omp_ms", "max_abs_error", "rmse"):
            require(np.allclose(image_check[f"{column}_raw"], image_check[f"{column}_csv"],
                                rtol=0, atol=FLOAT_TOLERANCE),
                    f"{run_name}: {column} in metrics_per_image.csv differs from raw timings")
        require((images[["max_abs_error", "rmse"]].to_numpy(dtype=float) == 0).all(),
                f"{run_name}: sequential and OpenMP outputs are not pixel-identical")

        calculated = images.groupby(SUMMARY_KEYS, as_index=False).agg(
            images=("image", "size"),
            seq_ms=("seq_ms", "median"),
            omp_ms=("omp_ms", "median"),
            max_abs_error=("max_abs_error", "max"),
            max_rmse=("rmse", "max"),
            seq_q1_ms=("seq_ms", lambda values: values.quantile(0.25)),
            seq_q3_ms=("seq_ms", lambda values: values.quantile(0.75)),
            omp_q1_ms=("omp_ms", lambda values: values.quantile(0.25)),
            omp_q3_ms=("omp_ms", lambda values: values.quantile(0.75)),
        )
        calculated["speedup"] = calculated.seq_ms / calculated.omp_ms
        calculated["efficiency_pct"] = 100 * calculated.speedup / calculated.threads
        summary_check = calculated.merge(summary, on=SUMMARY_KEYS, suffixes=("_raw", "_csv"),
                                         validate="one_to_one", indicator=True)
        require((summary_check["_merge"] == "both").all(), f"{run_name}: summary keys differ")
        for column in ("images", "seq_ms", "omp_ms", "max_abs_error", "max_rmse",
                       "seq_q1_ms", "seq_q3_ms", "omp_q1_ms", "omp_q3_ms",
                       "speedup", "efficiency_pct"):
            require(np.allclose(summary_check[f"{column}_raw"], summary_check[f"{column}_csv"],
                                rtol=0, atol=FLOAT_TOLERANCE),
                    f"{run_name}: {column} in summary.csv differs from per-image data")
        require((calculated.images == 20).all(), f"{run_name}: expected 20 images per group")

        summary["run"] = run_number
        summary["started"] = config["started"]
        images["run"] = run_number
        summaries.append(summary)
        per_image.append(images)
        unique_time_samples = (timings[["algorithm", "size", "image"]].drop_duplicates().shape[0]
                               * len(SEQ_COLUMNS) + len(timings) * len(OMP_COLUMNS))
        audit.append({
            "experiment": "main",
            "run": run_number,
            "folder": run_name,
            "started": config["started"],
            "status": "complete (validated from full CSV coverage)",
            "timing_rows": len(timings),
            "comparisons": len(images),
            "unique_time_samples": unique_time_samples,
            "max_abs_error": timings.max_abs_error.max(),
            "max_rmse": timings.rmse.max(),
        })

    require(all(signature == signatures[0] for signature in signatures),
            "Official run configurations differ; refusing to combine them")
    return (pd.concat(summaries, ignore_index=True),
            pd.concat(per_image, ignore_index=True), pd.DataFrame(audit))


def draw_charts(summary):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter, LogLocator

    charts = HERE / "charts"
    charts.mkdir(exist_ok=True)
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 12})
    for algorithm, rows in summary.groupby("algorithm"):
        for slug, field, label in (
            ("time", "omp_ms", "Thời gian (ms)"),
            ("speedup", "speedup", "Hệ số tăng tốc (lần)"),
            ("efficiency", "efficiency_pct", "Hiệu suất song song (%)"),
        ):
            fig, ax = plt.subplots(figsize=(8.3, 4.5), layout="constrained")
            x_ticks = None
            for (size, group), marker in zip(rows.groupby("size"), ("o", "s", "^")):
                group = group.sort_values("threads")
                x = group.threads.tolist()
                y = group[field].tolist()
                if slug == "time":
                    x = [0, *x]
                    y = [group.seq_ms.iloc[0], *y]
                x_ticks = x
                ax.plot(x, y, marker=marker, linewidth=2, markersize=5.5,
                        label=f"{size} × {size}")
            ax.set_xticks(x_ticks, ["Seq" if value == 0 else str(value) for value in x_ticks])
            ax.set_xlabel("Cấu hình thực thi" if slug == "time" else "Số luồng OpenMP")
            ax.set_ylabel(label)
            if slug == "time":
                ax.set_yscale("log")
                ax.yaxis.set_major_locator(LogLocator(base=10, subs=(1, 2, 5), numticks=15))
                ax.yaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}"))
            else:
                ax.set_ylim(bottom=0)
            if slug == "efficiency":
                ax.axhline(100, linestyle="--", linewidth=1, color="grey", label="Hiệu suất lý tưởng")
            ax.grid(True, which="major", linestyle="--", alpha=0.3)
            ax.spines[["top", "right"]].set_visible(False)
            ax.legend(fontsize=10)
            fig.savefig(charts / f"{algorithm.lower()}_{slug}.png", dpi=300)
            plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-plots", action="store_true",
                        help="validate and regenerate CSV files without Matplotlib")
    args = parser.parse_args()

    by_run, per_image, audit = load_runs()
    by_run.to_csv(HERE / "main_by_run.csv", index=False)
    per_image.to_csv(HERE / "main_image_medians_by_run.csv", index=False)
    summary = by_run.groupby(SUMMARY_KEYS, as_index=False).agg(
        images=("images", "first"),
        seq_ms=("seq_ms", "median"),
        omp_ms=("omp_ms", "median"),
        seq_min_ms=("seq_ms", "min"),
        seq_max_ms=("seq_ms", "max"),
        omp_min_ms=("omp_ms", "min"),
        omp_max_ms=("omp_ms", "max"),
        max_abs_error=("max_abs_error", "max"),
        max_rmse=("max_rmse", "max"),
    )
    summary["speedup"] = summary.seq_ms / summary.omp_ms
    summary["efficiency_pct"] = 100 * summary.speedup / summary.threads
    summary["runs"] = len(RUN_NAMES)
    summary.to_csv(HERE / "main_summary_3runs.csv", index=False, float_format="%.9f")
    audit.to_csv(HERE / "validation.csv", index=False)
    if not args.no_plots:
        draw_charts(summary)
    print("Validated three official runs: 37,800 unique time samples, 3,240 comparisons, zero errors.")
    print(f"Updated summaries and validation in {HERE}")


if __name__ == "__main__":
    main()
