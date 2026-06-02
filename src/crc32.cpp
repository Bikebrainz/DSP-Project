#include "emcast/fec.hpp"

#include <array>

namespace emcast {

namespace {

// Build the standard reflected CRC-32 lookup table at startup.
std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> table{};
    constexpr std::uint32_t kPoly = 0xEDB88320u;
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (kPoly ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> table = make_crc_table();
    return table;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t len) {
    const auto& table = crc_table();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace emcast
