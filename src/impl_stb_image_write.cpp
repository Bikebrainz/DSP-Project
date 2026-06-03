// Single translation unit that compiles the stb_image_write implementation.
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // stb uses sprintf; silence MSVC deprecation
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO_DEPRECATION
#include "stb_image_write.h"
