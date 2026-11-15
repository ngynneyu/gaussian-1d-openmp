#!/usr/bin/env python3
"""Build and run the same benchmark for the three image filters (GNU/OpenMP)."""

import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import subprocess
import time
from datetime import datetime

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    "gaussian": ("ngynneyu/gaussian-1d-openmp", "2f639af25dd36171978e106c9f467e8a4263101d"),
    "laplacian": ("vietquang8071/ttss", "91e2ed2ac05a56f2dc8bc258f60028242777b977"),
}
FLAGS = ["-O3", "-fopenmp", "-ffp-contract=off", "-Wall", "-Wextra"]


def run(*args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def source_dirs():
    for name, (repo, commit) in PINS.items():
        directory = Path(".bench-cache") / name
        if not directory.exists():
            run("git", "clone", "--filter=blob:none", "--no-checkout",
                f"https://github.com/{repo}.git", str(directory))
            run("git", "-C", str(directory), "sparse-checkout", "set", "--cone",
                "gaussian_1d/src" if name == "gaussian" else "laplacian_openmp")
            run("git", "-C", str(directory), "checkout", "--detach", commit)
        head = run("git", "-C", str(directory), "rev-parse", "HEAD",
                   capture_output=True, text=True).stdout.strip()
        if head != commit:
            raise SystemExit(f"{directory} must be checked out at {commit}, found {head}")
        changes = run("git", "-C", str(directory), "status", "--porcelain", "--untracked-files=no",
                      capture_output=True, text=True).stdout.strip()
        if changes:
            raise SystemExit(f"{directory} has local changes:\n{changes}")
        yield directory


def build(cc, cxx):
    gaussian, laplacian = source_dirs()
    gsrc = gaussian / "gaussian_1d" / "src"
    lsrc = laplacian / "laplacian_openmp"
    build_dir = Path(".bench-cache/build")
    build_dir.mkdir(parents=True, exist_ok=True)
    objects = []
    units = [gsrc / name for name in ("gaussian.c", "gaussian_omp.c", "image_io.c")]
    units += [Path("scripts/gaussian_adapter.c"), Path("scripts/laplacian_adapter.c")]
    for source in units:
        obj = str(build_dir / f"{source.stem}.o")
        run(cc, "-std=gnu11", *FLAGS, "-I", str(gsrc), "-I", str(lsrc), "-c", str(source), "-o", obj)
        objects.append(obj)
    exe = build_dir / ("image_benchmark.exe" if os.name == "nt" else "image_benchmark")
    run(cxx, "-std=c++17", *FLAGS, "sobel_seq.cpp", "sobel_omp.cpp", "scripts/benchmark.cpp",
        *objects, "-o", str(exe))
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
    for size in (512, 1024, 2048):
        for index in range(1, 21):
            name = f"{size}/image_{index:02}.bin"
            file = directory / name
            if file.stat().st_size != size * size * 4:
                raise ValueError(f"Unexpected byte count in {file}")
            digest.update(name.encode("ascii") + b"\0")
            digest.update(file.read_bytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--dataset", type=Path, default=Path("data/processed"))
    parser.add_argument("--out", type=Path, default=Path("results/benchmark"))
    args = parser.parse_args()
    os.chdir(ROOT)
    digest = dataset_digest(args.dataset)
    exe = build(args.cc, args.cxx)
    args.out.mkdir(parents=True)
    env = dict(os.environ, OMP_DYNAMIC="FALSE", OMP_WAIT_POLICY="PASSIVE",
               OMP_THREAD_LIMIT="20", OMP_PROC_BIND="FALSE", GOMP_SPINCOUNT="0")
    for name in ("GOMP_CPU_AFFINITY", "OMP_PLACES"):
        env.pop(name, None)
    config = {
        "started": datetime.now().astimezone().isoformat(timespec="seconds"),
        "cpu": cpu_name(), "logical_processors": os.cpu_count(), "platform": platform.platform(),
        "c_compiler": run(args.cc, "--version", capture_output=True, text=True).stdout.splitlines()[0],
        "cpp_compiler": run(args.cxx, "--version", capture_output=True, text=True).stdout.splitlines()[0],
        "flags": FLAGS, "threads": [4, 8, 12, 16, 20], "warmup": 1, "repeats": 10,
        "environment": {key: env[key] for key in ("OMP_DYNAMIC", "OMP_WAIT_POLICY",
                       "OMP_THREAD_LIMIT", "OMP_PROC_BIND", "GOMP_SPINCOUNT")},
        "dataset_sha256": digest, "sources": PINS,
    }
    config_path = args.out / "run_config.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    start = time.monotonic()
    run(str(exe), str(args.dataset), str(args.out), env=env)
    config["wall_seconds"] = round(time.monotonic() - start, 2)
    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
