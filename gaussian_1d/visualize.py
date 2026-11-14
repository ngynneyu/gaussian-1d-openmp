"""Create side-by-side grayscale input and Gaussian output previews."""

import argparse
from array import array
from pathlib import Path
import sys

from PIL import Image, ImageDraw


def preview(source, filtered, output):
    with Image.open(filtered) as image:
        width, height = image.size
        pixels = array("f")
        with source.open("rb") as stream:
            pixels.fromfile(stream, width * height)
            if stream.read(1):
                raise ValueError(f"Unexpected data after image pixels: {source}")
        if sys.byteorder != "little":
            pixels.byteswap()
        original = Image.frombytes("L", image.size, bytes(int(value + 0.5) for value in pixels))
        panel = Image.new("L", (2 * width + 12, height + 32), 240)
        panel.paste(original, (0, 32))
        panel.paste(image, (width + 12, 32))
        labels = ImageDraw.Draw(panel)
        labels.text((8, 10), "Input grayscale", fill=20)
        labels.text((width + 20, 10), "Gaussian blur", fill=20)
        output.parent.mkdir(parents=True, exist_ok=True)
        panel.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset", type=Path)
    parser.add_argument("results", type=Path)
    args = parser.parse_args()
    images = sorted((args.results / "images").glob("*/seq/*.pgm"))
    if not images:
        parser.error("No Sequential PGM outputs found in the results directory")
    for image in images:
        size = image.parent.parent.name
        source = args.dataset / size / image.with_suffix(".bin").name
        output = args.results / "previews" / size / f"{image.stem}_preview.png"
        preview(source, image, output)
    print(f"Wrote {len(images)} previews to {args.results / 'previews'}")


if __name__ == "__main__":
    main()
