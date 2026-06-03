// emcast command-line interface.
//
//   emcast send  <file> --out signal.wav     encode a file to a WAV waveform
//   emcast recv  <signal.wav> --out file      decode a WAV back to a file
//   emcast tx    <file>                       transmit a file over the speakers
//   emcast rx    --out file --seconds N       record from the mic and decode
//   emcast loopback <file> --snr 12           encode -> AWGN -> decode (+ report)
//   emcast info  <signal.wav>                 inspect a waveform / frame
#include <CLI/CLI.hpp>
#include <cstdint>
#include <iostream>
#include <string>

#include "emcast/emcast.hpp"

namespace {

// Physical-layer options shared by every subcommand.
struct ModemOptions {
    std::string scheme = "bfsk";
    std::string profile;  // empty = use the individual options below
    std::uint32_t sample_rate = 48000;
    std::uint32_t samples_per_symbol = 48;
    double freq_space = 2000.0;
    double freq_mark = 3000.0;
    std::uint32_t preamble = 64;

    emcast::ModemConfig to_config() const {
        emcast::ModemConfig cfg;
        cfg.sample_rate = sample_rate;
        cfg.samples_per_symbol = samples_per_symbol;
        cfg.freq_space = freq_space;
        cfg.freq_mark = freq_mark;
        cfg.preamble_bits = preamble;
        if (!profile.empty()) {
            // A profile overrides the symbol-level settings (keeps sample_rate).
            emcast::ModemConfig p;
            if (!emcast::profile_from_string(profile, p))
                throw emcast::Error("unknown profile: " + profile +
                                    " (expected balanced|robust|fast)");
            cfg.samples_per_symbol = p.samples_per_symbol;
            cfg.freq_space = p.freq_space;
            cfg.freq_mark = p.freq_mark;
            cfg.preamble_bits = p.preamble_bits;
        }
        return cfg;
    }

    emcast::Scheme to_scheme() const {
        emcast::Scheme s;
        if (!emcast::scheme_from_string(scheme, s))
            throw emcast::Error("unknown scheme: " + scheme + " (expected bfsk|ook)");
        return s;
    }
};

void add_modem_options(CLI::App* app, ModemOptions& o) {
    app->add_option("--scheme", o.scheme, "Modulation scheme (bfsk|ook)")->capture_default_str();
    app->add_option("--profile", o.profile, "Preset (balanced|robust|fast); overrides symbol options");
    app->add_option("--sample-rate", o.sample_rate, "Sample rate (Hz)")->capture_default_str();
    app->add_option("--sps", o.samples_per_symbol, "Samples per symbol")->capture_default_str();
    app->add_option("--freq-space", o.freq_space, "Tone for bit 0 (Hz)")->capture_default_str();
    app->add_option("--freq-mark", o.freq_mark, "Tone for bit 1 (Hz)")->capture_default_str();
    app->add_option("--preamble", o.preamble, "Preamble bits")->capture_default_str();
}

int count_bit_errors(const emcast::Bytes& a, const emcast::Bytes& b) {
    std::size_t n = std::min(a.size(), b.size());
    int bits = 0;
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
    CLI::App app{"emcast — data-over-signal transmission toolkit"};
    app.set_version_flag("--version", std::string("emcast ") + emcast::kVersion);
    app.require_subcommand(1);

    ModemOptions mo;

    // send
    auto* send = app.add_subcommand("send", "Encode a file into a WAV waveform");
    std::string send_in, send_out = "signal.wav";
    send->add_option("input", send_in, "Input file")->required()->check(CLI::ExistingFile);
    send->add_option("-o,--out", send_out, "Output WAV")->capture_default_str();
    add_modem_options(send, mo);

    // recv
    auto* recv = app.add_subcommand("recv", "Decode a WAV waveform back into a file");
    std::string recv_in, recv_out = "recovered.bin";
    recv->add_option("input", recv_in, "Input WAV")->required()->check(CLI::ExistingFile);
    recv->add_option("-o,--out", recv_out, "Output file")->capture_default_str();
    add_modem_options(recv, mo);

    // tx (over the air)
    auto* tx = app.add_subcommand("tx", "Transmit a file over the speakers");
    std::string tx_in;
    tx->add_option("input", tx_in, "Input file")->required()->check(CLI::ExistingFile);
    add_modem_options(tx, mo);

    // rx (over the air)
    auto* rx = app.add_subcommand("rx", "Record from the microphone and decode");
    std::string rx_out = "recovered.bin";
    double rx_seconds = 10.0;
    rx->add_option("-o,--out", rx_out, "Output file")->capture_default_str();
    rx->add_option("-s,--seconds", rx_seconds, "Seconds to record")->capture_default_str();
    add_modem_options(rx, mo);

    // loopback (simulation)
    auto* loop = app.add_subcommand("loopback", "Encode -> AWGN -> decode and report");
    std::string loop_in, loop_out;
    double loop_snr = 1e9;  // effectively noiseless unless set
    loop->add_option("input", loop_in, "Input file")->required()->check(CLI::ExistingFile);
    loop->add_option("--snr", loop_snr, "Channel SNR in dB (omit for noiseless)");
    loop->add_option("-o,--out", loop_out, "Optional: write recovered payload here");
    add_modem_options(loop, mo);

    // info
    auto* info = app.add_subcommand("info", "Inspect a WAV waveform / embedded frame");
    std::string info_in;
    info->add_option("input", info_in, "Input WAV")->required()->check(CLI::ExistingFile);
    add_modem_options(info, mo);

    CLI11_PARSE(app, argc, argv);

    try {
        const emcast::ModemConfig cfg = mo.to_config();
        const emcast::Scheme scheme = mo.to_scheme();

        if (*send) {
            emcast::Transmitter t(cfg, scheme);
            emcast::Samples wave = t.encode_file(send_in);
            emcast::write_wav(send_out, wave, cfg.sample_rate);
            std::cout << "Encoded " << send_in << " -> " << send_out << " ("
                      << wave.size() << " samples, "
                      << static_cast<double>(wave.size()) / cfg.sample_rate << " s)\n";
        } else if (*recv) {
            std::uint32_t sr = 0;
            emcast::Samples wave = emcast::read_wav(recv_in, sr);
            emcast::Receiver r(cfg, scheme);
            std::string name;
            emcast::DecodeStatus st = r.decode_to_file(wave, recv_out, &name);
            std::cout << "Decode status: " << emcast::to_string(st) << "\n";
            if (st != emcast::DecodeStatus::Ok) return 2;
            std::cout << "Recovered " << recv_out;
            if (!name.empty()) std::cout << " (original name: " << name << ")";
            std::cout << "\n";
        } else if (*tx) {
            emcast::Transmitter t(cfg, scheme);
            emcast::Samples wave = t.encode_file(tx_in);
            std::cout << "Transmitting " << tx_in << " ("
                      << static_cast<double>(wave.size()) / cfg.sample_rate
                      << " s) over the speakers...\n";
            emcast::play_audio(wave, cfg.sample_rate);
            std::cout << "Done.\n";
        } else if (*rx) {
            std::cout << "Listening for " << rx_seconds << " s...\n";
            emcast::Samples wave = emcast::record_audio(rx_seconds, cfg.sample_rate);
            emcast::Receiver r(cfg, scheme);
            std::string name;
            emcast::DecodeStatus st = r.decode_to_file(wave, rx_out, &name);
            std::cout << "Decode status: " << emcast::to_string(st) << "\n";
            if (st != emcast::DecodeStatus::Ok) return 2;
            std::cout << "Recovered " << rx_out;
            if (!name.empty()) std::cout << " (original name: " << name << ")";
            std::cout << "\n";
        } else if (*loop) {
            emcast::Bytes payload = emcast::read_file(loop_in);
            emcast::Transmitter t(cfg, scheme);
            emcast::Samples wave = t.encode_bytes(payload, loop_in);
            emcast::Samples channel =
                (loop_snr < 1e8) ? emcast::add_awgn(wave, loop_snr) : wave;

            emcast::Receiver r(cfg, scheme);
            emcast::FrameInfo got;
            emcast::DecodeStatus st = r.decode_samples(channel, got);

            // Raw (pre-FEC) error estimate: compare demodulated vs transmitted frame.
            auto modem = emcast::make_modem(scheme, cfg);
            emcast::Bytes tx_frame = emcast::encode_frame(payload, loop_in);
            emcast::Bytes rx_frame = modem->demodulate(channel);
            int raw_bit_err = count_bit_errors(tx_frame, rx_frame);
            std::size_t tx_bits = tx_frame.size() * 8;

            std::cout << "Input bytes      : " << payload.size() << "\n";
            std::cout << "SNR              : "
                      << (loop_snr < 1e8 ? std::to_string(loop_snr) + " dB" : "noiseless") << "\n";
            std::cout << "Raw channel BER  : " << raw_bit_err << "/" << tx_bits;
            if (tx_bits) std::cout << " (" << (100.0 * raw_bit_err / tx_bits) << "%)";
            std::cout << "\n";
            std::cout << "Decode status    : " << emcast::to_string(st) << "\n";
            bool exact = (st == emcast::DecodeStatus::Ok && got.payload == payload);
            std::cout << "Recovered exactly: " << (exact ? "YES" : "no") << "\n";
            if (!loop_out.empty() && st == emcast::DecodeStatus::Ok) {
                emcast::write_file(loop_out, got.payload);
                std::cout << "Wrote " << loop_out << "\n";
            }
            return exact ? 0 : 3;
        } else if (*info) {
            std::uint32_t sr = 0;
            emcast::Samples wave = emcast::read_wav(info_in, sr);
            std::cout << "File          : " << info_in << "\n";
            std::cout << "Sample rate   : " << sr << " Hz\n";
            std::cout << "Samples       : " << wave.size() << "\n";
            std::cout << "Duration      : " << static_cast<double>(wave.size()) / (sr ? sr : 1)
                      << " s\n";
            emcast::Receiver r(cfg, scheme);
            emcast::FrameInfo fi;
            emcast::DecodeStatus st = r.decode_samples(wave, fi);
            std::cout << "Frame status  : " << emcast::to_string(st) << "\n";
            if (st == emcast::DecodeStatus::Ok) {
                std::cout << "Payload bytes : " << fi.payload.size() << "\n";
                std::cout << "Embedded name : " << (fi.filename.empty() ? "(none)" : fi.filename)
                          << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
