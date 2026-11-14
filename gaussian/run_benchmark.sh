#!/usr/bin/env bash
set -euo pipefail

variant_dir="$1"
mode="$2"
shift 2
shared_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
default_dataset="$variant_dir/../data/processed_raw_names"
if [[ ! -d "$default_dataset" ]]; then
    default_dataset="$variant_dir/../data/processed"
fi
dataset_dir="${DATASET_DIR:-$default_dataset}"
results_dir="${RESULTS_DIR:-$variant_dir/results}"
export_images="${EXPORT_IMAGES:-1}"
args=()

case "$export_images" in
    0|1) ;;
    *) echo "EXPORT_IMAGES must be 0 or 1." >&2; exit 2 ;;
esac

if [[ -n "${KERNEL_SIZE:-}" || -n "${SIGMA:-}" ]]; then
    args+=("${KERNEL_SIZE:-19}" "${SIGMA:-3}")
fi

if [[ "$mode" == single ]]; then
    if (( $# > 2 )); then
        echo "Usage: bash test_one.sh [512|1024|2048] [image.bin]" >&2
        exit 2
    fi
    size="${1:-1024}"
    case "$size" in
        512|1024|2048) ;;
        *) echo "Size must be 512, 1024 or 2048." >&2; exit 2 ;;
    esac
    if (( $# == 2 )); then
        image="$2"
    else
        shopt -s nullglob
        files=("$dataset_dir/$size/"*.bin)
        if (( ${#files[@]} == 0 )); then
            echo "No .bin images in $dataset_dir/$size" >&2
            exit 1
        fi
        image="${files[0]##*/}"
    fi
    if [[ "$image" == */* || "$image" != *.bin || ! -f "$dataset_dir/$size/$image" ]]; then
        echo "Input image not found: $dataset_dir/$size/$image" >&2
        exit 1
    fi
    args+=(--size "$size" --image "$image")
    output_parent="$results_dir/single/$size"
else
    if (( $# > 2 )); then
        echo "Usage: bash run_full.sh [dataset_dir] [results_dir]" >&2
        exit 2
    fi
    dataset_dir="${1:-$dataset_dir}"
    output_parent="${2:-$results_dir}/full"
fi

if [[ "$export_images" == 0 ]]; then
    args+=(--no-images)
elif ! python3 -c 'import PIL' >/dev/null 2>&1; then
    echo "PNG export requires Pillow. Install it or set EXPORT_IMAGES=0." >&2
    exit 1
fi

mkdir -p -- "$variant_dir/build"
# Keep the build and benchmark together so another run cannot replace the binary.
exec 9>"$variant_dir/build/.benchmark.lock"
flock 9
bash "$variant_dir/build_gcc.sh"
mkdir -p -- "$output_parent"
output="$output_parent/run_$(date +%Y%m%d_%H%M%S_%N)"
"$variant_dir/build/gaussian_bench" "$dataset_dir" "$output" "${args[@]}"
exec 9>&-
python3 "$shared_dir/summarize_results.py" "$output"
if [[ "$export_images" == 1 ]]; then
    python3 "$shared_dir/export_png.py" "$output"
    python3 "$shared_dir/visualize.py" "$dataset_dir" "$output"
fi
if [[ "$mode" == full ]] && python3 -c 'import matplotlib' >/dev/null 2>&1; then
    python3 "$shared_dir/plot_results.py" "$output"
fi
printf '\nResults: %s\nSummary: %s\n' "$output" "$output/summary.csv"
