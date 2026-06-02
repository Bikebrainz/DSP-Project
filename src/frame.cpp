#include "emcast/frame.hpp"

#include <algorithm>

#include "emcast/fec.hpp"

namespace emcast {
namespace {

constexpr std::uint8_t kMagic0 = 0xE3;
constexpr std::uint8_t kMagic1 = 0xC1;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kMaxNameLen = 200;
// Sanity cap so a corrupted-yet-CRC-passing header can't request a huge alloc.
constexpr std::uint32_t kMaxPayload = 256u * 1024u * 1024u;  // 256 MiB

void put_u32_le(Bytes& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

std::uint32_t get_u32_le(const Bytes& b, std::size_t off) {
    return static_cast<std::uint32_t>(b[off]) |
           (static_cast<std::uint32_t>(b[off + 1]) << 8) |
           (static_cast<std::uint32_t>(b[off + 2]) << 16) |
           (static_cast<std::uint32_t>(b[off + 3]) << 24);
}

// Strip directory components from a path, keeping just the file name.
std::string base_name(const std::string& path) {
    std::size_t pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    if (name.size() > kMaxNameLen) name.resize(kMaxNameLen);
    return name;
}

}  // namespace

Bytes encode_frame(const Bytes& payload, const std::string& filename) {
    std::string name = base_name(filename);

    Bytes msg;
    msg.push_back(kMagic0);
    msg.push_back(kMagic1);
    msg.push_back(kVersion);
    msg.push_back(0);  // flags (reserved)
    msg.push_back(static_cast<std::uint8_t>(name.size()));
    msg.insert(msg.end(), name.begin(), name.end());
    put_u32_le(msg, static_cast<std::uint32_t>(payload.size()));
    // header_crc covers everything written so far.
    std::uint32_t hcrc = crc32(msg);
    put_u32_le(msg, hcrc);
    // payload + its own CRC.
    msg.insert(msg.end(), payload.begin(), payload.end());
    put_u32_le(msg, crc32(payload));

    // RS-encode in 223-byte chunks (last chunk zero-padded).
    ReedSolomon rs(kBlockTotal - kBlockData);
    Bytes frame;
    for (std::size_t off = 0; off < msg.size(); off += kBlockData) {
        Bytes chunk(msg.begin() + static_cast<std::ptrdiff_t>(off),
                    msg.begin() + static_cast<std::ptrdiff_t>(std::min(off + kBlockData, msg.size())));
        chunk.resize(kBlockData, 0);  // pad final block
        Bytes code = rs.encode(chunk);
        frame.insert(frame.end(), code.begin(), code.end());
    }
    return frame;
}

DecodeStatus decode_frame(const Bytes& frame_bytes, FrameInfo& out) {
    if (frame_bytes.size() < kBlockTotal) return DecodeStatus::Truncated;

    ReedSolomon rs(kBlockTotal - kBlockData);

    // Decode block 0 to read the header.
    Bytes block0(frame_bytes.begin(), frame_bytes.begin() + kBlockTotal);
    Bytes data0;
    if (!rs.decode(block0, data0)) return DecodeStatus::BadHeader;

    if (data0.size() < 5 || data0[0] != kMagic0 || data0[1] != kMagic1)
        return DecodeStatus::BadHeader;
    if (data0[2] != kVersion) return DecodeStatus::BadHeader;
    std::size_t name_len = data0[4];
    std::size_t header_fixed = 2 + 1 + 1 + 1 + name_len + 4;  // up to & incl orig_len
    if (data0.size() < header_fixed + 4) return DecodeStatus::BadHeader;

    std::uint32_t orig_len = get_u32_le(data0, 2 + 1 + 1 + 1 + name_len);
    std::uint32_t hcrc = get_u32_le(data0, header_fixed);
    if (crc32(data0.data(), header_fixed) != hcrc) return DecodeStatus::BadHeader;
    if (orig_len > kMaxPayload) return DecodeStatus::BadHeader;

    std::string filename(data0.begin() + 5, data0.begin() + 5 + static_cast<std::ptrdiff_t>(name_len));

    // Total logical message length and number of codeword blocks.
    std::size_t msg_len = header_fixed + 4 + orig_len + 4;  // +header_crc +payload +payload_crc
    std::size_t n_blocks = (msg_len + kBlockData - 1) / kBlockData;
    if (frame_bytes.size() < n_blocks * kBlockTotal) return DecodeStatus::Truncated;

    // Decode every block and reassemble the logical message.
    Bytes msg;
    msg.reserve(n_blocks * kBlockData);
    for (std::size_t blk = 0; blk < n_blocks; ++blk) {
        Bytes code(frame_bytes.begin() + static_cast<std::ptrdiff_t>(blk * kBlockTotal),
                   frame_bytes.begin() + static_cast<std::ptrdiff_t>((blk + 1) * kBlockTotal));
        Bytes data;
        if (!rs.decode(code, data)) return DecodeStatus::TooManyErrors;
        msg.insert(msg.end(), data.begin(), data.end());
    }
    msg.resize(msg_len);

    std::size_t payload_off = header_fixed + 4;
    Bytes payload(msg.begin() + static_cast<std::ptrdiff_t>(payload_off),
                  msg.begin() + static_cast<std::ptrdiff_t>(payload_off + orig_len));
    std::uint32_t pcrc = get_u32_le(msg, payload_off + orig_len);
    if (crc32(payload) != pcrc) return DecodeStatus::PayloadCrcFail;

    out.filename = filename;
    out.payload = std::move(payload);
    return DecodeStatus::Ok;
}

}  // namespace emcast
