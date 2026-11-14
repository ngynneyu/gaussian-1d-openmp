"""Convert benchmark image outputs from PGM to lossless grayscale PNG."""

import argparse
from pathlib import Path
from tempfile import NamedTemporaryFile

from PIL import Image


def convert(source):
    target = source.with_suffix(".png")
    # Publish the PNG only after Pillow has written it successfully.
    with NamedTemporaryFile(dir=target.parent, suffix=".tmp", delete=False) as handle:
        temp = Path(handle.name)
    try:
        with Image.open(source) as image:
            image.save(temp, format="PNG")
        temp.replace(target)
    finally:
        temp.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="Directory produced by gaussian_bench")
    args = parser.parse_args()
    images = sorted((args.results / "images").glob("*/*/*.pgm"))
    if not images:
        parser.error(f"No benchmark images found under {args.results / 'images'}. "
                     "Build the current version and run the benchmark first.")

    for source in images:
        convert(source)
    print(f"Exported {len(images)} PNG files under {args.results / 'images'}")


if __name__ == "__main__":
    main()
