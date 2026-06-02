#include "emcast/types.hpp"

namespace emcast {

const char* to_string(DecodeStatus status) {
    switch (status) {
        case DecodeStatus::Ok: return "ok";
        case DecodeStatus::NoSync: return "no sync marker found";
        case DecodeStatus::BadHeader: return "bad/corrupt header";
        case DecodeStatus::TooManyErrors: return "too many errors to correct";
        case DecodeStatus::PayloadCrcFail: return "payload CRC mismatch";
        case DecodeStatus::Truncated: return "signal truncated";
    }
    return "unknown";
}

}  // namespace emcast
