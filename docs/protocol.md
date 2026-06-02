# On-wire protocol (v1)

A transmission has two nested structures: the **bit-level frame** produced by
the modem, and the **byte-level message** carried inside it.

## Bit-level frame (physical layer)

Emitted by `Bfsk::modulate`, in order:

| Field | Size | Purpose |
|-------|------|---------|
| Preamble | `preamble_bits` (default 64) | Alternating `0101…`; lets the receiver settle gain and find symbol timing. |
| Sync marker | 32 bits | CCSDS Attached Sync Marker `0x1ACFFC1D`; byte-aligns the stream. Located by correlation, tolerating ≤ 2 bit errors. |
| Frame bytes | variable | The RS-coded message below, MSB first. |

Each bit is one symbol, held for `samples_per_symbol` samples (default 48 →
1000 baud at 48 kHz), with continuous phase across symbols. How a bit maps to a
symbol depends on the scheme:

- **BFSK** (default): a `1` is the **mark** tone (default 3000 Hz), a `0` is the
  **space** tone (default 2000 Hz).
- **OOK**: the mark tone is gated **on** for a `1` and **off** for a `0`.

Both ends must use the same scheme (`--scheme`).

## Byte-level message (application layer)

The logical message, before Reed–Solomon coding:

```
offset  size            field
------  --------------  -----------------------------------------------
0       2               magic = 0xE3 0xC1
2       1               version = 1
3       1               flags (reserved, 0)
4       1               name_len (0..200)
5       name_len        file name (UTF-8, no path components)
5+nl    4               orig_len  (uint32, little-endian) — payload size
9+nl    4               header_crc (uint32 LE) — CRC-32 of bytes [0 .. 9+nl)
13+nl   orig_len        payload (the original file bytes)
13+nl+L 4               payload_crc (uint32 LE) — CRC-32 of the payload
```

(`nl` = `name_len`, `L` = `orig_len`.)

## Reed–Solomon coding

The logical message is split into **223-byte data chunks**; each chunk is
RS(255,223)-encoded into a **255-byte codeword** (the final chunk is
zero-padded). The frame bytes are the concatenation of these codewords.

Because the header lives at the start of the message, it is always inside
codeword block 0. A receiver therefore:

1. RS-decodes block 0 and parses the header (verifying magic + header CRC).
2. Computes the total message length from `orig_len` and the number of blocks.
3. RS-decodes every block, reassembles the message, and truncates to length.
4. Verifies the payload CRC and returns the payload + file name.

Each block independently tolerates up to **16 corrupted bytes**.

## Parameters both ends must share

`sample_rate`, `samples_per_symbol`, `freq_space`, `freq_mark` and
`preamble_bits` (the `ModemConfig`). The CLI exposes these as `--sample-rate`,
`--sps`, `--freq-space`, `--freq-mark` and `--preamble`. The RS geometry
(255,223) and the sync marker are fixed in v1.
