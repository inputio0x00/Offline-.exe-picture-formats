#include "image.h"

#include <cstdio>
#include <cstring>

#include <tiffio.h>
#include <tiffio.hxx>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace ic {

namespace {

#if defined(_WIN32)
// libtiff on Windows ships TIFFOpenW for wide paths.
TIFF* tiff_open_utf8(const std::string& utf8_path, const char* mode) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8_path.data(), (int)utf8_path.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_path.data(), (int)utf8_path.size(), w.data(), n);
    return TIFFOpenW(w.c_str(), mode);
}
#else
TIFF* tiff_open_utf8(const std::string& utf8_path, const char* mode) {
    return TIFFOpen(utf8_path.c_str(), mode);
}
#endif

void tiff_silence(const char*, const char*, va_list) { /* drop */ }

} // namespace

bool decode_tiff(const std::string& path, Image& out, std::string& err) {
    TIFFSetWarningHandler(nullptr);
    TIFFSetErrorHandler(nullptr);

    TIFF* tif = tiff_open_utf8(path, "r");
    if (!tif) { err = "cannot open TIFF"; return false; }

    uint32_t w = 0, h = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    if (w == 0 || h == 0) { TIFFClose(tif); err = "invalid TIFF dimensions"; return false; }

    out.width = (int)w;
    out.height = (int)h;
    // TIFFReadRGBAImage produces ABGR in 32-bit words, which on
    // little-endian == [R,G,B,A] bytes. Decode to a temp buffer (it stores
    // bottom-up by default) then flip to top-down.
    std::vector<uint32_t> buf((size_t)w * (size_t)h);
    if (!TIFFReadRGBAImage(tif, w, h, buf.data(), 0)) {
        TIFFClose(tif);
        err = "TIFFReadRGBAImage failed";
        return false;
    }

    out.pixels.resize((size_t)w * (size_t)h * 4);
    bool any_alpha = false;
    for (uint32_t y = 0; y < h; ++y) {
        const uint32_t* src = buf.data() + (size_t)(h - 1 - y) * w;
        uint8_t* dst = out.pixels.data() + (size_t)y * out.stride();
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t px = src[x];
            uint8_t r = (uint8_t)TIFFGetR(px);
            uint8_t g = (uint8_t)TIFFGetG(px);
            uint8_t b = (uint8_t)TIFFGetB(px);
            uint8_t a = (uint8_t)TIFFGetA(px);
            dst[x * 4 + 0] = r;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = b;
            dst[x * 4 + 3] = a;
            if (a != 0xFF) any_alpha = true;
        }
    }
    out.has_alpha = any_alpha;

    TIFFClose(tif);
    return true;
}

bool encode_tiff(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err) {
    if (!img.valid()) { err = "empty image"; return false; }

    TIFFSetWarningHandler(nullptr);
    TIFFSetErrorHandler(nullptr);

    TIFF* tif = tiff_open_utf8(path, "w");
    if (!tif) { err = "cannot create TIFF"; return false; }

    int channels = img.has_alpha ? 4 : 3;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32_t)img.width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t)img.height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, channels);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));

    if (img.has_alpha) {
        uint16_t extra[1] = { EXTRASAMPLE_UNASSALPHA };
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, (uint16_t)1, extra);
    }

    int comp_choice = o.tiff_compression;
    int tiff_comp = COMPRESSION_LZW;
    switch (comp_choice) {
        case 0: tiff_comp = COMPRESSION_NONE; break;
        case 1: tiff_comp = COMPRESSION_LZW; break;
        case 2: tiff_comp = COMPRESSION_ADOBE_DEFLATE; break;
        case 3: tiff_comp = COMPRESSION_PACKBITS; break;
        default: tiff_comp = COMPRESSION_LZW; break;
    }
    TIFFSetField(tif, TIFFTAG_COMPRESSION, tiff_comp);
    if (tiff_comp == COMPRESSION_ADOBE_DEFLATE) {
        TIFFSetField(tif, TIFFTAG_ZIPQUALITY, 6);
    }

    std::vector<uint8_t> row((size_t)img.width * (size_t)channels);
    for (int y = 0; y < img.height; ++y) {
        const uint8_t* src = img.pixels.data() + (size_t)y * img.stride();
        if (channels == 4) {
            std::memcpy(row.data(), src, row.size());
        } else {
            for (int x = 0; x < img.width; ++x) {
                row[x * 3 + 0] = src[x * 4 + 0];
                row[x * 3 + 1] = src[x * 4 + 1];
                row[x * 3 + 2] = src[x * 4 + 2];
            }
        }
        if (TIFFWriteScanline(tif, row.data(), (uint32_t)y, 0) < 0) {
            TIFFClose(tif);
            err = "TIFFWriteScanline failed";
            return false;
        }
    }

    TIFFClose(tif);
    return true;
}

} // namespace ic
