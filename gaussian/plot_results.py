"""Plot Gaussian benchmark metrics from a complete full or single-image run."""

import argparse
import csv
from pathlib import Path
from statistics import median

COLORS = ("#0072B2", "#D55E00", "#009E73")


def load_groups(folder):
    config = dict(line.split("=", 1) for line in
                  (folder / "run_config.txt").read_text(encoding="utf-8").splitlines())
    with (folder / "metrics.csv").open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source))

    threads = tuple(map(int, config["threads"].split(",")))
    sizes = tuple(map(int, config["sizes"].split(",")))
    configs = {("seq", 1), *(("omp", count) for count in threads)}
    images = {}
    for row in rows:
        size = int(row["width"])
        if size not in sizes or size != int(row["height"]):
            raise ValueError("Unexpected image size in metrics.csv")
        if row["kernel_size"] != config["kernel_size"] or row["sigma"] != config["sigma"]:
            raise ValueError("Mixed kernel settings in metrics.csv")
        key = row["implementation"], int(row["threads"])
        entries = images.setdefault((size, row["image"]), {})
        if key in entries:
            raise ValueError(f"Duplicate configuration for {row['image']} at {size}")
        entries[key] = row

    expected = 20 if config["image"] == "all" else 1
    if len(images) != len(sizes) * expected or any(entries.keys() != configs for entries in images.values()):
        raise ValueError("Incomplete benchmark results")
    groups = {}
    for size in sizes:
        subset = {name: entries for (side, name), entries in images.items() if side == size}
        if len(subset) != expected or (expected == 1 and config["image"] not in subset):
            raise ValueError(f"Incomplete benchmark results for {size}")
        groups[size] = list(subset.values())
    return config, groups, threads


def plot(folder, config, groups, threads):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    charts = (
        ("execution_time", "Execution time", "Median execution time (ms)", "median_ms"),
        ("speedup", "Speedup", "Median speedup (×)", "speedup"),
        ("parallel_efficiency", "Parallel efficiency", "Median efficiency (%)", "efficiency_pct"),
    )
    image_count = len(next(iter(groups.values())))
    description = "1 image per size" if image_count == 1 else f"{image_count} images per size"
    for filename, title, ylabel, field in charts:
        fig, ax = plt.subplots(figsize=(8.2, 5.0), layout="constrained")
        for (size, images), color in zip(groups.items(), COLORS):
            values = [median(float(image["omp", count][field]) for image in images)
                      for count in threads]
            ax.plot(threads, values, "o-", color=color, lw=2, label=f"{size} × {size}")
            if field == "median_ms":
                baseline = median(float(image["seq", 1][field]) for image in images)
                ax.axhline(baseline, color=color, lw=1.1, linestyle="--", alpha=0.8)
        if field == "median_ms":
            ax.plot([], [], color="#555555", linestyle="--", label="Sequential baseline")
        elif field == "speedup":
            ax.plot(threads, threads, color="#777777", linestyle="--", label="Ideal speedup")
        else:
            ax.axhline(100, color="#777777", linestyle="--", label="100% efficiency")
        ax.set_title(title, loc="left", fontsize=17, fontweight="bold", pad=28)
        kernel = config["kernel_size"]
        ax.text(0, 1.02, f"{kernel} × {kernel} kernel  |  σ = {config['sigma']}  |  {description}",
                transform=ax.transAxes, color="#46525e", fontsize=10)
        ax.set(xlabel="OpenMP threads", ylabel=ylabel, xticks=threads)
        ax.set_ylim(bottom=0)
        ax.grid(axis="y", alpha=0.23)
        ax.spines[["top", "right"]].set_visible(False)
        ax.legend(frameon=False, ncols=2)
        fig.savefig(folder / f"{filename}.png", dpi=180)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    args = parser.parse_args()
    config, groups, threads = load_groups(args.results)
    plot(args.results, config, groups, threads)


if __name__ == "__main__":
    main()
