"""Save PNG previews of the filter outputs that passed comparison."""

from csv import DictReader
from pathlib import Path
from shutil import copyfile

from PIL import Image
from PIL.ImageMath import lambda_eval


def image_filenames(dataset, image_ids):
    defaults = {image_id: f"{image_id}.png" for image_id in image_ids}
    metadata = dataset / "metadata.csv"
    if not metadata.is_file():
        return defaults

    original_names = {}
    with metadata.open(encoding="utf-8-sig", newline="") as file:
        for row in DictReader(file):
            image_id = row.get("image_id")
            original_file = row.get("original_file")
            if image_id not in defaults or not original_file:
                continue
            filename = Path(original_file).with_suffix(".png").name
            if original_names.setdefault(image_id, filename) != filename:
                return defaults

    names = {**defaults, **original_names}
    if len({name.casefold() for name in names.values()}) != len(names):
        return defaults
    return names


def load_preview(source, algorithm, size):
    data = source.read_bytes()
    dimensions = (size, size)
    if algorithm == "Gaussian":
        return Image.frombytes("L", dimensions, data)

    with Image.frombytes("F", dimensions, data, "raw", "F;32F") as edges:
        # Use a fixed display scale across images and show both signs of Laplacian.
        with lambda_eval(lambda images: abs(images["edges"]) / 4, edges=edges) as magnitude:
            return magnitude.convert("L")


def export_images(raw_dir, image_dir, sizes, filenames, threads):
    versions = ("seq", *(f"omp{count}" for count in threads))
    count = 0
    for algorithm in ("Sobel", "Gaussian", "Laplacian"):
        for size in sizes:
            size_dir = image_dir / algorithm / str(size)
            for version in versions:
                (size_dir / version).mkdir(parents=True)
            for image_id, filename in filenames.items():
                source = raw_dir / f"{algorithm}_{size}_{image_id}.bin"
                preview = size_dir / "seq" / filename
                with load_preview(source, algorithm, size) as image:
                    image.save(preview, compress_level=3)
                # The benchmark verifies each OpenMP output matches the sequential image.
                for version in versions[1:]:
                    copyfile(preview, size_dir / version / filename)
                count += len(versions)
    return count
