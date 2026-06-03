# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and this project adheres to
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Pluggable modulation schemes behind a `Modem` interface with a `make_modem`
  factory; shared framing/sync/timing scaffolding in `modem_common`.
- **OOK** (on-off keying) scheme alongside BFSK, selectable via `--scheme`.
- `emcast_bench` BER-vs-SNR benchmark tool (CSV output) and
  `docs/benchmarks.md` with representative results.
- Round-trip tests for every scheme.
- CMake `install()` + exported package config so downstream projects can
  `find_package(emcast)` and link `emcast::emcast`; CPack ZIP packaging.
- Modem profiles (`balanced`/`robust`/`fast`) as `ModemConfig` presets and a
  `--profile` CLI option.
- Robustness/fuzz tests: random noise and random bytes never crash the decoder
  and are rejected cleanly; every profile round-trips.
- Three new modulation schemes: **DBPSK** and **DQPSK** (differential PSK on one
  carrier, via a new complex single-bin DFT `goertzel_iq`) and **MFSK** (4-ary
  FSK). DQPSK and MFSK carry 2 bits/symbol (double throughput). Shared receiver
  timing search factored into `search_and_pack`.
- Spectrogram analysis (`compute_spectrogram`), an `emcast spectrogram` command
  that writes a colormapped PNG (stb_image_write), and a zero-dependency browser
  viewer at `demo/index.html` (waveform + live spectrogram, client-side).

## [1.0.0] — unreleased

The project was rebuilt from a 2023 proof-of-concept into a full data-over-signal
transmission toolkit.

### Added
- Layered modem stack: framing, Reed–Solomon FEC, BFSK modulation, Goertzel
  demodulation with preamble + CCSDS sync.
- `ReedSolomon` codec over GF(2⁸); default RS(255,223) corrects 16 byte errors
  per block.
- CRC-32 integrity checks on both the header and the payload.
- Channels: WAV file I/O (dr_wav), AWGN simulator, real speaker/microphone I/O
  (miniaudio).
- `Transmitter` / `Receiver` SDK facade and a `read_file`/`write_file` helper.
- `emcast` CLI: `send`, `recv`, `tx`, `rx`, `loopback`, `info`.
- Catch2 test suite (CRC, RS, framing, Goertzel, end-to-end round-trip through
  AWGN) and GitHub Actions CI across Linux, macOS and Windows.
- CMake build with automatic dependency fetching and presets.

### Changed
- The original 2023 C prototype is preserved under `legacy/`.
- Removed committed build artifacts and the multi-megabyte text waveform dumps.
