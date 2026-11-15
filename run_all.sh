#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 0 && ( $# -ne 1 || $1 != --images ) ]]; then
    printf 'Usage: %s [--images]\n' "$0" >&2
    exit 2
fi

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
out="results/wsl_$(date +%Y%m%d_%H%M%S_%N)"

python3 benchmark/run.py --cc "${CC:-gcc}" --out "$out" "$@"
python3 benchmark/summarize.py "$out"

printf 'Results: %s/%s\n' "$PWD" "$out"
