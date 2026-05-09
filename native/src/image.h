// Internal RGBA8 image representation passed between decoders and encoders.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ic {

struct Image {
    int width  = 0;
    int height = 0;
    // Always 4 channels, interleaved RGBA, 8 bits per channel,
    // stride = width * 4. Pixels are non-premultiplied.
    std::vector<uint8_t> pixels;
    bool has_alpha = false;

    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
    size_t stride() const { return static_cast<size_t>(width) * 4; }
};

// Format detection by magic bytes. Falls back to extension if magic is
// inconclusive. Returns one of the IC_FMT_* values.
int detect_format_utf8(const std::string& utf8_path);

// On Windows, libpng/libjpeg/libtiff/libheif/libwebp open files via fopen()
// which doesn't grok UTF-8 paths. Open the file ourselves (using _wfopen
// after converting from UTF-8 to wide) and pass the FILE* in.
//
// Returns nullptr on failure. Caller closes with fclose().
FILE* fopen_utf8(const std::string& utf8_path, const char* mode);

// Decoders. Each returns true on success.
bool decode_jpg(const std::string& path, Image& out, std::string& err);
bool decode_png(const std::string& path, Image& out, std::string& err);
bool decode_webp(const std::string& path, Image& out, std::string& err);
bool decode_tiff(const std::string& path, Image& out, std::string& err);
bool decode_heif(const std::string& path, Image& out, std::string& err);

// Encoders.
struct EncodeOptions {
    int quality = 90;          // 1..100
    int png_compression = 6;   // 0..9
    bool lossless = false;
    bool jpeg_progressive = false;
    int tiff_compression = 1;  // 0=none 1=LZW 2=ZIP 3=PackBits
};

bool encode_jpg(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err);
bool encode_png(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err);
bool encode_webp(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err);
bool encode_tiff(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err);
bool encode_heif(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err);

} // namespace ic
