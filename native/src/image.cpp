#include "image.h"
#include "../include/img_convert.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace ic {

#if defined(_WIN32)
static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
#endif

FILE* fopen_utf8(const std::string& utf8_path, const char* mode) {
#if defined(_WIN32)
    auto wpath = utf8_to_wide(utf8_path);
    std::wstring wmode;
    for (const char* p = mode; *p; ++p) wmode.push_back(static_cast<wchar_t>(*p));
    FILE* f = nullptr;
    _wfopen_s(&f, wpath.c_str(), wmode.c_str());
    return f;
#else
    return std::fopen(utf8_path.c_str(), mode);
#endif
}

static std::string lower_ext(const std::string& path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = path.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return e;
}

int detect_format_utf8(const std::string& utf8_path) {
    FILE* f = fopen_utf8(utf8_path, "rb");
    if (f) {
        unsigned char buf[16] = {0};
        size_t n = std::fread(buf, 1, sizeof(buf), f);
        std::fclose(f);

        if (n >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) {
            return IC_FMT_JPG;
        }
        if (n >= 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' &&
            buf[3] == 'G' && buf[4] == 0x0D && buf[5] == 0x0A &&
            buf[6] == 0x1A && buf[7] == 0x0A) {
            return IC_FMT_PNG;
        }
        if (n >= 12 && std::memcmp(buf, "RIFF", 4) == 0 &&
            std::memcmp(buf + 8, "WEBP", 4) == 0) {
            return IC_FMT_WEBP;
        }
        if (n >= 4 && ((buf[0] == 'I' && buf[1] == 'I' && buf[2] == 0x2A && buf[3] == 0x00) ||
                       (buf[0] == 'M' && buf[1] == 'M' && buf[2] == 0x00 && buf[3] == 0x2A))) {
            return IC_FMT_TIFF;
        }
        // HEIF / HEIC / AVIF share an ISO BMFF container: bytes 4..7 == "ftyp"
        // followed by a brand. Common HEIF brands: heic, heix, mif1, msf1.
        if (n >= 12 && std::memcmp(buf + 4, "ftyp", 4) == 0) {
            const char* brand = reinterpret_cast<const char*>(buf + 8);
            if (std::memcmp(brand, "heic", 4) == 0 ||
                std::memcmp(brand, "heix", 4) == 0 ||
                std::memcmp(brand, "mif1", 4) == 0 ||
                std::memcmp(brand, "msf1", 4) == 0 ||
                std::memcmp(brand, "hevc", 4) == 0 ||
                std::memcmp(brand, "hevx", 4) == 0 ||
                std::memcmp(brand, "avif", 4) == 0) {
                return IC_FMT_HEIF;
            }
        }
    }

    // Fall back to extension. Useful when the file doesn't exist yet (output)
    // or for HEIF variants we didn't list above.
    std::string e = lower_ext(utf8_path);
    if (e == "jpg" || e == "jpeg" || e == "jpe" || e == "jfif") return IC_FMT_JPG;
    if (e == "png") return IC_FMT_PNG;
    if (e == "webp") return IC_FMT_WEBP;
    if (e == "tif" || e == "tiff") return IC_FMT_TIFF;
    if (e == "heif" || e == "heic" || e == "hif" || e == "avif") return IC_FMT_HEIF;
    return IC_FMT_UNKNOWN;
}

} // namespace ic
