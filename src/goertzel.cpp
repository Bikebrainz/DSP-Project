#include "emcast/modem.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {

double goertzel_power(const Sample* samples, std::size_t n, double freq,
                      double sample_rate) {
    if (n == 0) return 0.0;
    // Use the exact target frequency (tones are placed on Goertzel bins by
    // construction, so this is both accurate and well-conditioned).
    const double w = 2.0 * M_PI * freq / sample_rate;
    const double coeff = 2.0 * std::cos(w);
    double s_prev = 0.0;
    double s_prev2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double s = static_cast<double>(samples[i]) + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }
    return s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
}

std::complex<double> goertzel_iq(const Sample* samples, std::size_t n, double freq,
                                 double sample_rate) {
    // Direct single-bin DFT: re = sum x cos, im = -sum x sin.
    const double w = 2.0 * M_PI * freq / sample_rate;
    double re = 0.0;
    double im = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(samples[i]);
        re += x * std::cos(w * static_cast<double>(i));
        im -= x * std::sin(w * static_cast<double>(i));
    }
    return {re, im};
}

}  // namespace emcast
