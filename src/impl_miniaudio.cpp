// Single translation unit that compiles the miniaudio implementation.
// We only need raw device playback/capture, so decoding/encoding are disabled
// to keep the build lean.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
