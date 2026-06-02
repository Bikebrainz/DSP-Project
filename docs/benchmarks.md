# Benchmarks: BER vs SNR

`emcast_bench` sweeps channel SNR for a modulation scheme, pushing random
payloads through `modulate → AWGN → demodulate → decode` and reporting two
metrics as CSV:

- **raw channel BER** — bit errors at the demodulator output, *before* FEC
  (measured over frames where the sync marker was found).
- **frame success rate** — fraction of trials decoded to a bit-exact payload
  with a valid CRC, *after* Reed–Solomon correction. This is the metric that
  matters end-to-end.

## Running it

```bash
emcast_bench --scheme bfsk --bytes 512 --trials 40 --snr-min -6 --snr-max 8 --snr-step 2
emcast_bench --scheme ook  --bytes 512 --trials 40 --snr-min -6 --snr-max 14 --snr-step 2
```

Output is CSV on stdout (pipe to a file and plot, or import into a spreadsheet).

## Representative results

512-byte payloads, 20 trials per point, default profile (1000 baud, 48 kHz),
RS(255,223). **Frame success rate**:

| SNR (dB) | BFSK | OOK |
|---------:|:----:|:---:|
| −6 | 0%   | 0%   |
| −4 | 100% | 0%   |
| −2 | 100% | 0%   |
|  0 | 100% | 0%   |
|  2 | 100% | 55%  |
|  4 | 100% | 100% |
|  6 | 100% | 100% |

**Takeaways**

- The Reed–Solomon layer turns a low-but-nonzero raw BER into *exact* recovery:
  BFSK runs at ~0.4% raw BER at −4 dB yet every frame decodes perfectly.
- BFSK is roughly **8 dB more robust** than OOK — the expected gap between
  non-coherent FSK and amplitude keying. OOK is offered for throughput/spectral
  comparisons and teaching, not for hostile channels.
- There is a sharp "cliff": once the raw BER exceeds what RS(255,223) can absorb
  (~16 bytes per 255-byte block), success drops from 100% to 0% quickly. Pick an
  operating SNR with margin above the cliff.
