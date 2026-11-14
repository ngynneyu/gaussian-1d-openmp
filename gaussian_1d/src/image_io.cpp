#include "image_io.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace gaussian {

int parse_integer(const char* text) {
    const std::string input(text);
    std::size_t end = 0;
    const int number = std::stoi(input, &end);
    if (end != input.size()) {
        throw std::invalid_argument("Invalid integer: " + input);
    }
    return number;
}

double parse_number(const char* text) {
    const std::string input(text);
    std::size_t end = 0;
    const double number = std::stod(input, &end);
    if (end != input.size()) {
        throw std::invalid_argument("Invalid number: " + input);
    }
    return number;
}

Image read_image(const fs::path& path, int width, int height) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "Dataset requires IEEE 754 float32");
    const std::uint32_t endian_test = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian_test) != 1) {
        throw std::runtime_error("Dataset reader requires a little-endian host");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }
    const auto count = std::uintmax_t(width) * std::uintmax_t(height);
    const auto bytes = count * sizeof(float);
    Image image{width, height, {}};
    if (count > image.pixels.max_size() ||
        bytes > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("Image is too large");
    }
    if (fs::file_size(path) != bytes) {
        throw std::runtime_error("File size does not match width * height * 4: " + path.string());
    }
    image.pixels.resize(static_cast<std::size_t>(count));
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(bytes))) {
        throw std::runtime_error("Cannot read image: " + path.string());
    }
    for (float pixel : image.pixels) {
        if (!std::isfinite(pixel) || pixel < 0.0f || pixel > 255.0f) {
            throw std::runtime_error("Expected finite grayscale pixels in [0, 255]: " + path.string());
        }
    }
    return image;
}

void write_pgm(const fs::path& path, const Image& image, const Pixels& pixels) {
    if (fs::symlink_status(path).type() != fs::file_type::not_found) {
        throw std::runtime_error("Output already exists: " + path.string());
    }
    std::ofstream file(path, std::ios::binary);
    file << "P5\n" << image.width << ' ' << image.height << "\n255\n";
    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    file.close();
    if (!file) {
        throw std::runtime_error("Cannot write PGM: " + path.string());
    }
}

}  // namespace gaussian
