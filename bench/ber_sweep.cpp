// emcast BER-vs-SNR benchmark.
//
// Sweeps channel SNR for a chosen modulation scheme, pushing random payloads
// through modulate -> AWGN -> demodulate -> decode, and reports the raw channel
// bit-error rate and the end-to-end frame success rate as CSV.
//
//   emcast_bench --scheme bfsk --bytes 512 --trials 40 \
//                --snr-min 0 --snr-max 20 --snr-step 2
#include <CLI/CLI.hpp>
#include <iostream>
#include <random>
#include <string>

#include "emcast/emcast.hpp"

namespace {

emcast::Bytes random_bytes(std::mt19937& rng, std::size_t n) {
    emcast::Bytes b(n);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& x : b) x = static_cast<std::uint8_t>(d(rng));
    return b;
}

long count_bit_errors(const emcast::Bytes& a, const emcast::Bytes& b) {
    std::size_t n = std::min(a.size(), b.size());
    long bits = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint8_t x = a[i] ^ b[i];
        while (x) {
            bits += x & 1;
            x >>= 1;
        }
    }
    return bits;
}

}  // namespace

int main(int argc, char** argv) {
    CLI::App app{"emcast BER-vs-SNR benchmark"};
    std::string scheme_name = "bfsk";
    std::size_t bytes = 512;
    int trials = 40;
    double snr_min = 0.0, snr_max = 20.0, snr_step = 2.0;
    emcast::ModemConfig cfg;

    app.add_option("--scheme", scheme_name, "Modulation scheme (bfsk|ook)")->capture_default_str();
    app.add_option("--bytes", bytes, "Payload size per trial")->capture_default_str();
    app.add_option("--trials", trials, "Trials per SNR point")->capture_default_str();
    app.add_option("--snr-min", snr_min, "Minimum SNR (dB)")->capture_default_str();
    app.add_option("--snr-max", snr_max, "Maximum SNR (dB)")->capture_default_str();
    app.add_option("--snr-step", snr_step, "SNR step (dB)")->capture_default_str();
    app.add_option("--sps", cfg.samples_per_symbol, "Samples per symbol")->capture_default_str();
    app.add_option("--sample-rate", cfg.sample_rate, "Sample rate (Hz)")->capture_default_str();
    CLI11_PARSE(app, argc, argv);

    emcast::Scheme scheme;
    if (!emcast::scheme_from_string(scheme_name, scheme)) {
        std::cerr << "error: unknown scheme '" << scheme_name << "'\n";
        return 1;
    }
    auto modem = emcast::make_modem(scheme, cfg);

    std::cerr << "# scheme=" << scheme_name << " baud=" << cfg.baud()
              << " bytes=" << bytes << " trials=" << trials << "\n";
    std::cout << "scheme,snr_db,payload_bytes,trials,raw_ber_pct,frame_success_pct\n";

    int snr_index = 0;
    for (double snr = snr_min; snr <= snr_max + 1e-9; snr += snr_step, ++snr_index) {
        long raw_err = 0, raw_bits = 0, successes = 0;
        for (int t = 0; t < trials; ++t) {
            std::mt19937 rng(static_cast<std::uint32_t>(1000 * snr_index + t));
            emcast::Bytes payload = random_bytes(rng, bytes);
            emcast::Bytes tx_frame = emcast::encode_frame(payload, "bench.bin");
            emcast::Samples wave = modem->modulate(tx_frame);
            emcast::Samples noisy = emcast::add_awgn(wave, snr, rng());

            emcast::Bytes rx_frame = modem->demodulate(noisy);
            if (!rx_frame.empty()) {
                raw_err += count_bit_errors(tx_frame, rx_frame);
                raw_bits += static_cast<long>(tx_frame.size()) * 8;
            }
            emcast::FrameInfo info;
            if (emcast::decode_frame(rx_frame, info) == emcast::DecodeStatus::Ok &&
                info.payload == payload) {
                ++successes;
            }
        }
        double raw_ber = raw_bits ? (100.0 * raw_err / raw_bits) : 0.0;
        double success = 100.0 * successes / trials;
        std::cout << scheme_name << "," << snr << "," << bytes << "," << trials << ","
                  << raw_ber << "," << success << "\n";
    }
    return 0;
}
