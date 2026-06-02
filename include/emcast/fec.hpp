// emcast — forward error correction and integrity primitives.
#ifndef EMCAST_FEC_HPP
#define EMCAST_FEC_HPP

#include <cstddef>
#include <cstdint>

#include "emcast/types.hpp"

namespace emcast {

/// IEEE 802.3 CRC-32 (reflected, polynomial 0xEDB88320), the same variant
/// used by zlib/PNG. crc32("123456789") == 0xCBF43926.
std::uint32_t crc32(const std::uint8_t* data, std::size_t len);
inline std::uint32_t crc32(const Bytes& data) {
    return crc32(data.data(), data.size());
}

/// Systematic Reed-Solomon codec over GF(2^8) (primitive polynomial 0x11D).
///
/// A codeword is `data_len + parity_len` bytes; up to `parity_len / 2` byte
/// errors per codeword can be corrected. The default RS(255,223) tolerates 16
/// corrupted bytes in every 255-byte block.
class ReedSolomon {
public:
    explicit ReedSolomon(std::size_t parity_len = 32);

    /// Maximum data bytes per codeword (255 - parity_len).
    std::size_t max_data_len() const { return 255 - parity_len_; }
    std::size_t parity_len() const { return parity_len_; }

    /// Append `parity_len()` parity bytes to `data` (which must not exceed
    /// max_data_len()) and return the resulting codeword.
    Bytes encode(const Bytes& data) const;

    /// Decode/repair a codeword in place. Returns true if the message was
    /// already correct or successfully repaired, false if it had more errors
    /// than the code can correct. On success, `out` holds the recovered data
    /// (codeword minus parity).
    bool decode(const Bytes& codeword, Bytes& out) const;

private:
    std::size_t parity_len_;
};

}  // namespace emcast

#endif  // EMCAST_FEC_HPP
