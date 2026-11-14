#include "gaussian.hpp"

#include <omp.h>

namespace gaussian {

void configure_openmp() {
    omp_set_dynamic(0);
}

int openmp_version() {
    return _OPENMP;
}

int openmp_available_processors() {
    return omp_get_num_procs();
}

}  // namespace gaussian
