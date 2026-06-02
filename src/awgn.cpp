#include "emcast/channel.hpp"

#include <cmath>
#include <random>

namespace emcast {

Samples add_awgn(const Samples& in, double snr_db, std::uint32_t seed) {
    Samples out = in;
    if (in.empty()) return out;

    // Mean signal power.
    double power = 0.0;
    for (Sample v : in) power += static_cast<double>(v) * v;
    power /= static_cast<double>(in.size());
    if (power <= 0.0) return out;

    const double snr_linear = std::pow(10.0, snr_db / 10.0);
    const double noise_std = std::sqrt(power / snr_linear);

    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, noise_std);
    for (Sample& v : out) v = static_cast<Sample>(v + noise(rng));
    return out;
}

}  // namespace emcast
