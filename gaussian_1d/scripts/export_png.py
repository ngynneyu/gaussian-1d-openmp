"""Export benchmark PNGs and optionally create side-by-side input/output previews."""

import argparse
from array import array
from pathlib import Path
import sys
from tempfile import NamedTemporaryFile

from PIL import Image, ImageDraw


def save_png(image, target):
    with NamedTemporaryFile(dir=target.parent, suffix=".tmp", delete=False) as handle:
        temp = Path(handle.name)
    try:
        image.save(temp, format="PNG")
        temp.replace(target)
    finally:
        temp.unlink(missing_ok=True)


def save_preview(source, filtered, output):
    width, height = filtered.size
    pixels = array("f")
    with source.open("rb") as stream:
        pixels.fromfile(stream, width * height)
        if stream.read(1):
            raise ValueError(f"Unexpected data after image pixels: {source}")
    if sys.byteorder != "little":
        pixels.byteswap()
    original = Image.frombytes("L", filtered.size, bytes(int(value + 0.5) for value in pixels))
    panel = Image.new("L", (2 * width + 12, height + 32), 240)
    panel.paste(original, (0, 32))
    panel.paste(filtered, (width + 12, 32))
    labels = ImageDraw.Draw(panel)
    labels.text((8, 10), "Input grayscale", fill=20)
    labels.text((width + 20, 10), "Gaussian blur", fill=20)
    output.parent.mkdir(parents=True, exist_ok=True)
    save_png(panel, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="Directory produced by gaussian_bench")
    parser.add_argument("--dataset", type=Path, help="Processed dataset used for input previews")
    args = parser.parse_args()
    images = sorted((args.results / "images").glob("*/*/*.pgm"))
    if not images:
        parser.error(f"No benchmark images found under {args.results / 'images'}. "
                     "Build the current version and run the benchmark first.")

    preview_count = 0
    for source in images:
        with Image.open(source) as image:
            save_png(image, source.with_suffix(".png"))
            if args.dataset is not None and source.parent.name == "seq":
                size = source.parent.parent.name
                original = args.dataset / size / source.with_suffix(".bin").name
                preview = args.results / "previews" / size / f"{source.stem}_preview.png"
                save_preview(original, image, preview)
                preview_count += 1
    print(f"Exported {len(images)} PNG files under {args.results / 'images'}")
    if args.dataset is not None:
        print(f"Wrote {preview_count} previews to {args.results / 'previews'}")


if __name__ == "__main__":
    main()
