#!/usr/bin/env python3
"""Aggregate batch runs and draw sequential vs OpenMP (within each image)."""
import argparse
import csv
import json
import math
from pathlib import Path

FILTERS = ("Gaussian", "Sobel", "Laplacian")
MODES = (("seq", 0), ("intra", 16))
LABELS = ("Tuần tự", "OpenMP (16 luồng)")
COLORS = ("#777777", "#286A9E")
HATCHES = ("", "")


def fmt(value):
    return f"{value:.2f}".replace(".", ",")


def draw(results, output, count, dpi=300, kind="both"):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from matplotlib.ticker import MaxNLocator, FuncFormatter
    from matplotlib.patches import Patch

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10.5,
                         "axes.spines.top": False, "axes.spines.right": False,
                         "axes.labelsize": 11, "axes.titlesize": 12,
                         "legend.fontsize": 10.5, "hatch.linewidth": 0.8})
    output.mkdir(parents=True, exist_ok=True)
    names = list(results)
    handles = [Patch(facecolor=c, edgecolor="white", hatch=h, label=l)
               for c, h, l in zip(COLORS, HATCHES, LABELS)]

    def finish(fig, name):
        path = output / name
        fig.savefig(path, dpi=dpi, facecolor="white", bbox_inches="tight", pad_inches=.10)
        plt.close(fig)
        print(path)

    if kind in ("both", "time"):
        fig, axes = plt.subplots(1, len(names), figsize=(9.5 if len(names)>1 else 4.5, 2.9),
                                 squeeze=False)
        fig.subplots_adjust(left=.085, right=.985, bottom=.12, top=.71, wspace=.42)
        for ax, name in zip(axes[0], names):
            times = results[name][0]
            for j, v in enumerate(times):
                bars = ax.bar(j, v, width=.67, color=COLORS[j], hatch=HATCHES[j],
                              edgecolor="white", linewidth=.6, zorder=3)
                ax.bar_label(bars, labels=[fmt(v)], padding=3, fontsize=9.5)
            ax.set_title(name, fontweight="semibold", pad=6)
            ax.set_ylim(0, max(times)*1.30)
            ax.set_xlim(-.6, 1.6)
            ax.set_xticks([])
            ax.set_ylabel("Thời gian (ms)")
            ax.yaxis.set_major_locator(MaxNLocator(nbins=4))
            ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:.0f}"))
            ax.grid(axis="y", linestyle="--", alpha=.23)
            ax.set_axisbelow(True)
        fig.legend(handles=handles, loc="upper center", bbox_to_anchor=(.51,.98), ncol=2, frameon=False)
        finish(fig, "batch_all_time.png")

    if kind in ("both", "speedup"):
        fig, ax = plt.subplots(figsize=(8.4, 2.9))
        fig.subplots_adjust(left=.11, right=.98, bottom=.13, top=.76)
        x = np.arange(len(names))
        peak = max(s for a in names for s in results[a][1][1:])
        ceiling = max(8, 2*math.ceil(peak*1.12/2))
        for j in (1,):
            vals = [results[a][1][j] for a in names]
            bars = ax.bar(x, vals, .48, color=COLORS[j], hatch=HATCHES[j],
                          edgecolor="white", linewidth=.6, zorder=3)
            ax.bar_label(bars, labels=[fmt(v) for v in vals], padding=4, fontsize=10)
        ax.set_xticks(x, names)
        ax.set_xlim(-.55, len(names)-.45)
        ax.set_ylabel("Hệ số tăng tốc (lần)")
        ax.set_ylim(0, ceiling)
        ax.set_yticks(np.arange(0, ceiling+1, 2))
        ax.grid(axis="y", linestyle="--", alpha=.23)
        ax.set_axisbelow(True)
        fig.legend(handles=handles[1:], loc="upper center", bbox_to_anchor=(.55,.98),
                   ncol=1, frameon=False)
        finish(fig, "batch_all_speedup.png")


def read_runs(path):
    """Read run directories or ZIP members without extracting executables."""
    import zipfile
    import io
    if path.is_file() and zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for name in sorted(n for n in z.namelist() if n.endswith('/run_config.json') or n == 'run_config.json'):
                prefix = name[:-len('run_config.json')]
                def read(filename):
                    return z.read(prefix + filename).decode('utf-8-sig')
                yield prefix.rstrip('/') or path.stem, json.loads(read('run_config.json')), {f: list(csv.DictReader(io.StringIO(read(f)))) for f in ('timings.csv','summary.csv','errors_per_image.csv')}
    elif path.is_dir():
        paths = [path / 'run_config.json'] if (path / 'run_config.json').is_file() else sorted(path.rglob('run_config.json'))
        for config in paths:
            yield config.parent.name, json.loads(config.read_text(encoding='utf-8-sig')), {f: list(csv.DictReader((config.parent/f).read_text(encoding='utf-8-sig').splitlines())) for f in ('timings.csv','summary.csv','errors_per_image.csv')}
    else:
        raise ValueError(f'Expected a run folder or ZIP: {path}')


def aggregate(paths, count, output, expected_runs):
    from collections import defaultdict
    from statistics import median
    records, signatures, names = [], [], set()
    def require(condition, message):
        if not condition:
            raise ValueError(message)
    for path in paths:
        for name, c, files in read_runs(path):
            print(f'Checking {name}', flush=True)
            require(c.get('schema_version') == 2 and c.get('status') == 'complete', f'{name}: incomplete or incompatible run')
            require(c.get('benchmark_version') == 5, f'{name}: only V5 benchmark results are supported')
            require(name not in names, f'Duplicate run: {name}')
            names.add(name)
            algs = tuple(c['algorithms'])
            require(set(algs) <= set(FILTERS) and len(set(algs)) == len(algs) and bool(algs), 'Invalid algorithms')
            require(count in c['image_counts'], f'{name}: missing {count} images')
            require(set(MODES) <= {(m['strategy'], int(m['threads'])) for m in c['modes']}, f'{name}: required modes missing')
            # Require the same benchmark version and inputs within one aggregation.
            signatures.append({k: c.get(k) for k in ('benchmark_version','compiler','flags','source_sha256','algorithms','gaussian','image_counts','modes','repeats','warmup_batches_per_configuration','timer','timing_sampling','minimum_sample_seconds_requested','minimum_sample_ticks','order','omp_environment','dataset','manifest_sha256','platform','logical_processors','power_scheme')})
            samples = defaultdict(dict)
            for r in files['timings.csv']:
                mode = (r['strategy'], int(r['threads']))
                if int(r['image_count']) != count or mode not in MODES:
                    continue
                key = (r['algorithm'], *mode)
                rep = int(r['repeat']); seconds = float(r['total_seconds']); raw = float(r['elapsed_seconds']); batches = int(r['batches_per_sample'])
                require(rep not in samples[key] and batches > 0 and math.isfinite(seconds) and seconds > 0 and math.isfinite(raw) and raw >= float(c['minimum_sample_seconds'])-1e-12 and math.isclose(raw/batches, seconds, rel_tol=1e-10), f'{name}: invalid timing')
                samples[key][rep] = seconds * 1000
            keys = {(a,*m) for a in algs for m in MODES}
            require(set(samples) == keys and all(set(v) == set(range(1,int(c['repeats'])+1)) for v in samples.values()), f'{name}: missing timing samples')
            errors = {}
            for r in files['errors_per_image.csv']:
                mode = (r['strategy'],int(r['threads'])); i = int(r['image_index'])
                if mode != MODES[1] or i > count:
                    continue
                key = (r['algorithm'],i)
                require(key not in errors, f'{name}: duplicate error')
                errors[key] = (float(r['max_abs_error']),float(r['rmse']))
            require(set(errors) == {(a,i) for a in algs for i in range(1,count+1)} and all(p == (0,0) for p in errors.values()), f'{name}: missing comparisons or nonzero error')
            summaries = {}
            for r in files['summary.csv']:
                mode = (r['strategy'],int(r['threads']))
                if int(r['image_count']) != count or mode not in MODES:
                    continue
                key = (r['algorithm'],*mode)
                require(key not in summaries, f'{name}: duplicate summary')
                summaries[key] = float(r['median_ms'])
            require(set(summaries) == keys, f'{name}: summary rows missing')
            for (a,m,p), ms in summaries.items():
                require(math.isclose(ms,median(samples[a,m,p].values()),rel_tol=0,abs_tol=1.1e-6), f'{name}: median mismatch')
                seq = summaries[a,'seq',0]
                records.append(dict(run=name, algorithm=a, image_count=count, strategy=m, threads=p, median_ms=ms, speedup=seq/ms, efficiency_pct=100*seq/ms/p if p else '', timing_samples=len(samples[a,m,p]), comparisons=count if p else 0, max_abs_error=0, max_rmse=0))
    require(len(names) == expected_runs, f'Expected {expected_runs} valid runs, got {len(names)}. Select exact folders or use --runs 1 for a single run.')
    require(all(s == signatures[0] for s in signatures), 'Run configurations differ. Do not mix benchmark revisions.')
    output.mkdir(parents=True, exist_ok=True)
    def save(filename, rows):
        with (output/filename).open('w',encoding='utf-8',newline='') as f:
            writer = csv.DictWriter(f,fieldnames=list(rows[0]))
            writer.writeheader(); writer.writerows(rows)
    save('batch_by_run.csv',records)
    summary, results = [], {}
    for a in FILTERS:
        if a not in signatures[0]['algorithms']:
            continue
        times = [median(r['median_ms'] for r in records if r['algorithm']==a and (r['strategy'],r['threads'])==m) for m in MODES]
        speeds = [times[0]/t for t in times]
        results[a] = (times,speeds)
        for (m,p),t,s in zip(MODES,times,speeds):
            selected = [r for r in records if r['algorithm']==a and r['strategy']==m]
            summary.append(dict(algorithm=a,image_count=count,strategy=m,threads=p,median_ms=t,min_ms=min(r['median_ms'] for r in selected),max_ms=max(r['median_ms'] for r in selected),speedup=s,efficiency_pct=100*s/p if p else '',reduction_pct=100*(1-t/times[0]),timing_samples=sum(r['timing_samples'] for r in selected),comparisons=sum(r['comparisons'] for r in selected),max_abs_error=0,max_rmse=0,runs=len(names)))
    save(f'batch_summary_{len(names)}runs.csv',summary)
    print(f'Validated {len(names)} runs; retained {sum(r["timing_samples"] for r in records)} samples and {sum(r["comparisons"] for r in records)} comparisons.',flush=True)
    return results


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs',type=Path,nargs='+',help='ZIP with runs, or exact run folders')
    parser.add_argument('--runs',type=int,default=3,help='expected number of runs (default 3)')
    parser.add_argument('--count',type=int,default=100)
    parser.add_argument('--out',type=Path,help='output folder; default: batch_report beside script')
    parser.add_argument('--dpi',type=int,default=300)
    parser.add_argument('--metric',choices=('both','time','speedup'),default='both')
    args=parser.parse_args()
    if min(args.runs,args.count,args.dpi)<1:
        parser.error('runs, count and dpi must be positive')
    try:
        output=args.out or Path(__file__).resolve().parent/'batch_report'
        data=aggregate(args.inputs,args.count,output,args.runs)
        draw(data,output,args.count,args.dpi,args.metric)
        print(f'[DONE] {output.resolve()}',flush=True)
    except (OSError,ValueError,KeyError) as exc:
        parser.error(str(exc))


if __name__=='__main__':
    main()
