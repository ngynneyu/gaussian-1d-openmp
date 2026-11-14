#include "gaussian.hpp"

namespace gaussian {

void blur_sequential(const Image& input, const Kernel& kernel, Pixels& output) {
    for (int y = 0; y < input.height; ++y) {
        blur_row(input, kernel, output, y);
    }
}

}  // namespace gaussian
