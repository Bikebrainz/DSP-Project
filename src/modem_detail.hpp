// Internal helpers shared by the modulation schemes. Not part of the public API.
#ifndef EMCAST_SRC_MODEM_DETAIL_HPP
#define EMCAST_SRC_MODEM_DETAIL_HPP

#include <cstdint>
#include <vector>

#include "emcast/types.hpp"

namespace emcast {
namespace detail {

/// Expand bytes to bits, MSB first.
std::vector<std::uint8_t> bytes_to_bits(const Bytes& bytes);

/// The full transmit bit stream: alternating preamble + 32-bit sync + frame.
std::vector<std::uint8_t> build_bitstream(const ModemConfig& cfg, std::uint32_t sync_word,
                                          const Bytes& frame_bytes);

/// The 32 sync-marker bits, MSB first.
std::vector<std::uint8_t> sync_bits(std::uint32_t sync_word);

/// Normalize samples to peak magnitude 1 (defensive; safe on silence).
std::vector<Sample> normalize(const Samples& samples, std::size_t sps);

/// Index of the first sps-window whose energy exceeds 20% of the peak window,
/// backed off one symbol. Locates the signal start on a live recording.
std::size_t detect_start(const std::vector<Sample>& norm, std::size_t sps);

/// Locate the sync marker in `bits` (tolerating <= 2 bit errors) and pack the
/// bits that follow into bytes (MSB first). Empty if the marker is absent.
Bytes pack_after_sync(const std::vector<std::uint8_t>& bits, std::uint32_t sync_word);

}  // namespace detail
}  // namespace emcast

#endif  // EMCAST_SRC_MODEM_DETAIL_HPP
