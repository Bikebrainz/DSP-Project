# emcast — data-over-signal transmission toolkit

[![CI](https://github.com/Bikebrainz/DSP-Project/actions/workflows/ci.yml/badge.svg)](https://github.com/Bikebrainz/DSP-Project/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://isocpp.org/)

**emcast** encodes an arbitrary file into a modulated audio waveform, transmits
it over a channel — a WAV file, a simulated noisy link, or a real
**speaker → microphone** path — and decodes it back into a *bit-exact* copy of
the original. It is a small but complete software modem: framing, integrity
checks, forward error correction, modulation and synchronization.

```
file ─▶ frame (header + CRC) ─▶ Reed–Solomon FEC ─▶ BFSK modulator ─▶ ┐
                                                                      channel
recovered file ◀─ CRC verify ◀─ RS decode ◀─ Goertzel demod + sync ◀─ ┘
```

## Why

This project began in 2023 as a Digital Signal Processing class project by
Dean Urschel: a proof-of-concept that amplitude-modulated the bits of a JPEG
onto a sampled sine wave, dumped the result to a 3 MB human-readable text file,
and reconstructed the image on the far side. It worked, but it was brittle — no
error correction, no synchronization, fixed-column text parsing, hard-coded
file names.

`emcast` keeps that idea and makes it real. The original prototype is preserved
under [`legacy/`](legacy/).

## Quick start

### Build

Requires CMake ≥ 3.16 and a C++17 compiler. Dependencies (CLI11, Catch2,
miniaudio, dr_wav) are fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release      # run the test suite
```

On Windows, run the above from a *Developer PowerShell for VS 2022* (or use the
`msvc` CMake preset).

### Install & integrate

```bash
cmake --install build --prefix /your/prefix      # lib + headers + CLI + cmake package
cpack --config build/CPackConfig.cmake           # build a distributable ZIP
```

Then consume it from another CMake project:

```cmake
find_package(emcast REQUIRED)
target_link_libraries(your_app PRIVATE emcast::emcast)
```

### Profiles

Pick a ready-made trade-off with `--profile` (overrides the symbol-level
options): `balanced` (default, 1000 baud), `robust` (500 baud, longer preamble —
best over the air), or `fast` (2000 baud, needs a cleaner link).

```bash
emcast send photo.jpg --profile robust --out signal.wav
```

### Use the CLI

```bash
# Encode a file to a WAV waveform, then decode it back — bit for bit.
emcast send  photo.jpg --out signal.wav
emcast recv  signal.wav --out recovered.jpg

# Choose a modulation scheme (bfsk|dbpsk|dqpsk|mfsk|ook).
emcast send  photo.jpg --scheme dqpsk --out signal.wav   # 2 bits/symbol

# Prove reliability over a simulated noisy channel (encode → AWGN → decode).
emcast loopback photo.jpg --snr 8

# Inspect a waveform and its embedded frame.
emcast info signal.wav

# Transmit over the air: run the receiver on one machine, transmit on another.
emcast rx --out recovered.bin --seconds 30      # start listening
emcast tx photo.jpg                             # play through the speakers
```

### Use the library

```cpp
#include <emcast/emcast.hpp>

emcast::Transmitter tx;
emcast::Samples wave = tx.encode_file("photo.jpg");
emcast::write_wav("signal.wav", wave, tx.config().sample_rate);

emcast::Receiver rx;
std::uint32_t sr;
emcast::Samples in = emcast::read_wav("signal.wav", sr);
emcast::FrameInfo out;
if (rx.decode_samples(in, out) == emcast::DecodeStatus::Ok) {
    // out.payload == original file bytes; out.filename == "photo.jpg"
}
```

## How it works

| Layer | What it does |
|-------|--------------|
| **Framing** | Wraps the payload with a magic marker, version, file name, length, a header CRC-32 and a payload CRC-32. |
| **FEC** | Reed–Solomon over GF(2⁸). The default RS(255,223) corrects up to **16 corrupted bytes in every 255-byte block**. |
| **Modulation** | Five pluggable schemes behind one interface: **BFSK** (default), **DBPSK** (differential PSK, most robust), **DQPSK** & **MFSK** (2 bits/symbol — double throughput), and **OOK**. Default 1000 baud at 48 kHz; select with `--scheme`. |
| **Sync** | An alternating preamble for gain/timing, then a 32-bit CCSDS sync marker located by correlation. |
| **Demod** | Goertzel tone-power detection per symbol (cheaper than an FFT), with signal-start detection and symbol-timing search for live audio. |

See [`docs/architecture.md`](docs/architecture.md) and
[`docs/protocol.md`](docs/protocol.md) for the full design and the on-wire
format.

### Reliability

The full pipeline recovers an 8 KB JPEG bit-exactly even with dozens of channel
bit errors:

| Channel SNR | Raw channel bit errors | Recovered exactly? |
|------------:|-----------------------:|:------------------:|
| noiseless   | 0                      | ✅ |
| 8 dB        | 0                      | ✅ |
| −1 dB       | 2                      | ✅ (corrected by FEC) |
| −3 dB       | 82                     | ✅ (corrected by FEC) |

*(Reproduce with `emcast loopback examples/x-files.jpg --snr <dB>`.)*

For systematic BER-vs-SNR curves across schemes, see
[`docs/benchmarks.md`](docs/benchmarks.md) and the `emcast_bench` tool.

## Roadmap

- **v1:** framing + CRC + Reed–Solomon, BFSK modem, WAV / AWGN / live-audio
  channels, CLI, test suite, CI. ✅
- **v2 (in progress):** pluggable modulation schemes (BFSK + OOK ✅), BER-vs-SNR
  benchmark suite ✅; next: BPSK/QPSK & multitone, RRC pulse shaping, Gardner
  timing recovery, installable packages.
- **v3:** SDR/RF channel, live visualization demo, fuzzing, tagged 1.0 release.

## License

MIT — see [LICENSE](LICENSE). Originally created by Dean Urschel (2023);
developed into emcast by Bikebrainz.
