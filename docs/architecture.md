# Architecture

emcast is organized as a layered modem stack. Each layer has a narrow,
testable responsibility and a small public header under `include/emcast/`.

```
            ┌─────────────────────────────────────────────┐
  file ───▶ │ Transmitter (emcast.hpp)                     │
            │   encode_frame ─▶ Bfsk::modulate             │ ──▶ Samples
            └─────────────────────────────────────────────┘
                              │
                          Channel
                 (WAV file / AWGN sim / live audio)
                              │
            ┌─────────────────────────────────────────────┐
  file ◀─── │ Receiver (emcast.hpp)                        │
            │   Bfsk::demodulate ─▶ decode_frame           │ ◀── Samples
            └─────────────────────────────────────────────┘
```

## Components

### `types.hpp`
Shared primitives: `Bytes`, `Samples`, `ModemConfig` (sample rate, samples per
symbol, tone frequencies, preamble length, amplitude) and `DecodeStatus`.

### `fec.hpp` — integrity & error correction
- **`crc32`** — reflected IEEE 802.3 CRC-32 (the zlib/PNG variant).
- **`ReedSolomon`** — systematic RS over GF(2⁸), primitive polynomial `0x11D`,
  generator `2`. Encoding is synthetic division by the generator polynomial;
  decoding is syndrome calculation → Berlekamp–Massey → Chien search → Forney.
  The default RS(255,223) corrects up to 16 byte errors per 255-byte block.

### `frame.hpp` — application framing
Turns a payload + metadata into a sequence of RS codeword blocks (see
[`protocol.md`](protocol.md)). Block 0 always carries the header, so a receiver
can decode it first to learn how many further blocks to expect. Integrity is
checked twice: a header CRC and a payload CRC.

### `modem.hpp` — physical layer
- **`goertzel_power`** — single-bin Goertzel filter; the energy of a sample
  window at one frequency. Tones are placed exactly on Goertzel bins
  (`freq = k · sample_rate / samples_per_symbol`) so the two tones are
  orthogonal and detection is well-conditioned.
- **`Modem`** — abstract interface (`modulate` / `demodulate`); construct a
  concrete scheme with `make_modem(Scheme, ModemConfig)`. All schemes share the
  same bit-level framing (preamble + 32-bit sync + frame bytes) and the demod
  scaffolding (normalize → signal-start detection → symbol-timing search → sync
  search → byte packing), factored into `src/modem_common.cpp`. Only symbol
  rendering and the per-symbol bit decision differ per scheme.
- **`Bfsk`** — binary FSK: a `1` is the mark tone, a `0` the space tone, with
  continuous phase. The decision compares the two tone energies (amplitude
  agnostic). The robust default.
- **`Ook`** — on-off keying: the mark tone is gated on for a `1`, off for a `0`.
  The receiver thresholds per-symbol energy adaptively. Simpler but amplitude
  sensitive, so it needs ~8 dB more SNR than BFSK (see
  [`benchmarks.md`](benchmarks.md)).

### `channel.hpp` — transports
- **`add_awgn`** — additive white Gaussian noise at a target SNR; deterministic
  given a seed (used by tests and benchmarks).
- **`write_wav` / `read_wav`** — 16-bit PCM mono WAV I/O via dr_wav.
- **`play_audio` / `record_audio`** — real speaker/microphone I/O via miniaudio.

### `emcast.hpp` — SDK facade
`Transmitter` and `Receiver` compose the layers into one-call file↔waveform
operations, plus `read_file` / `write_file` helpers.

## Design choices

- **BFSK first.** Non-coherent FSK is the proven choice for an acoustic link: it
  tolerates amplitude variation (room volume, AGC) because the decision compares
  two tone energies rather than an absolute level.
- **Decision is amplitude-agnostic.** Comparing Goertzel powers means the
  receiver does not need precise gain calibration.
- **FEC everywhere, including the header.** Every block is RS-coded, so a
  corrupted header is recoverable, not fatal.
- **Defense in depth.** RS corrects, CRCs detect. A frame that decodes but fails
  its payload CRC is reported as `PayloadCrcFail` rather than returned as good.

## Testing

`tests/` (Catch2) covers each layer plus an end-to-end property test that pushes
random payloads through modulate → AWGN → demodulate → decode and asserts
bit-exact recovery. CI runs the suite on Linux, macOS and Windows.
