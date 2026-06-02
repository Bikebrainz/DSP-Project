// emcast — application framing: turns a payload + metadata into a sequence of
// Reed-Solomon codeword blocks (and back), with integrity checks.
//
// Logical message layout (before RS coding):
//   magic[2]=0xE3,0xC1 | version[1] | flags[1] | name_len[1] | name[name_len]
//   | orig_len[4 LE] | header_crc[4 LE] | payload[orig_len] | payload_crc[4 LE]
// header_crc covers every byte before it; payload_crc covers the raw payload.
//
// The message is split into 223-byte chunks, each RS-encoded into a 255-byte
// codeword; the frame is the concatenation of those codewords. Because block 0
// always contains the header, a receiver can decode it first to learn the total
// length and therefore how many further blocks to expect.
#ifndef EMCAST_FRAME_HPP
#define EMCAST_FRAME_HPP

#include <string>

#include "emcast/types.hpp"

namespace emcast {

/// Recovered frame contents.
struct FrameInfo {
    std::string filename;  ///< Original file name (may be empty).
    Bytes payload;         ///< Original file bytes.
};

/// RS data bytes per codeword block (k in RS(255, k)).
constexpr std::size_t kBlockData = 223;
/// Total codeword size in bytes (n in RS(n, 223)).
constexpr std::size_t kBlockTotal = 255;

/// Build a frame (concatenated RS codewords) carrying `payload`. `filename` is
/// stored as metadata (path components are stripped; over-long names trimmed).
Bytes encode_frame(const Bytes& payload, const std::string& filename);

/// Parse a frame produced by encode_frame. `frame_bytes` may contain trailing
/// garbage past the real frame (ignored). Returns DecodeStatus::Ok on success.
DecodeStatus decode_frame(const Bytes& frame_bytes, FrameInfo& out);

}  // namespace emcast

#endif  // EMCAST_FRAME_HPP
