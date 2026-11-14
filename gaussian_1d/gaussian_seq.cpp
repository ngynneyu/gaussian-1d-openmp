#include "gaussian_1d.hpp"

namespace gaussian::separable {

void blur_sequential(const Image& input, const Kernel& kernel,
                     std::vector<double>& intermediate, Pixels& output) {
    for (int y = 0; y < input.height; ++y) {
        blur_horizontal_row(input, kernel, intermediate, y);
    }
    for (int y = 0; y < input.height; ++y) {
        blur_vertical_row(input, kernel, intermediate, output, y);
    }
}

}  // namespace gaussian::separable
