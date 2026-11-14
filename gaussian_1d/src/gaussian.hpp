#pragma once

#include <cstdint>
#include <vector>

namespace gaussian {

using Pixels = std::vector<std::uint8_t>;

struct Image {
    int width;
    int height;
    std::vector<float> pixels;
};


void configure_openmp();
int openmp_version();
int openmp_available_processors();

}  // namespace gaussian
