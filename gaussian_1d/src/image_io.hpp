#pragma once

#include "gaussian.hpp"

#include <filesystem>

namespace gaussian {

int parse_integer(const char* text);
double parse_number(const char* text);
Image read_image(const std::filesystem::path& path, int width, int height);
void write_pgm(const std::filesystem::path& path, const Image& image, const Pixels& pixels);

}  // namespace gaussian
