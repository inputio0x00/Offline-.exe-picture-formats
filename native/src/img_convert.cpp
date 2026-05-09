// DLL entry points: dispatch to per-format codecs.
// IMG_CONVERT_BUILD_DLL is defined by CMake (target_compile_definitions).
#include "../include/img_convert.h"
#include "image.h"

#include <cstring>
#include <string>

namespace {

const char* k_version = "1.0.0";

void copy_err(char* buf, int size, const std::string& msg) {
    if (!buf || size <= 0) return;
    int n = (int)msg.size();
    if (n >= size) n = size - 1;
    std::memcpy(buf, msg.data(), (size_t)n);
    buf[n] = 0;
}

ic::EncodeOptions to_internal(const ic_options* o) {
    ic::EncodeOptions r;
    if (!o) return r;
    r.quality          = o->quality > 0 ? o->quality : 90;
    r.png_compression  = o->png_compression > 0 ? o->png_compression : 6;
    r.lossless         = o->lossless != 0;
    r.jpeg_progressive = o->jpeg_progressive != 0;
    r.tiff_compression = o->tiff_compression;
    return r;
}

bool decode_any(const std::string& path, int format, ic::Image& img, std::string& err) {
    switch (format) {
        case IC_FMT_JPG:  return ic::decode_jpg(path, img, err);
        case IC_FMT_PNG:  return ic::decode_png(path, img, err);
        case IC_FMT_WEBP: return ic::decode_webp(path, img, err);
        case IC_FMT_TIFF: return ic::decode_tiff(path, img, err);
        case IC_FMT_HEIF: return ic::decode_heif(path, img, err);
        default: err = "unknown source format"; return false;
    }
}

bool encode_any(const std::string& path, int format, const ic::Image& img,
                const ic::EncodeOptions& o, std::string& err) {
    switch (format) {
        case IC_FMT_JPG:  return ic::encode_jpg(path, img, o, err);
        case IC_FMT_PNG:  return ic::encode_png(path, img, o, err);
        case IC_FMT_WEBP: return ic::encode_webp(path, img, o, err);
        case IC_FMT_TIFF: return ic::encode_tiff(path, img, o, err);
        case IC_FMT_HEIF: return ic::encode_heif(path, img, o, err);
        default: err = "unsupported target format"; return false;
    }
}

} // namespace

extern "C" {

IC_API ic_format IC_CALL ic_detect_format(const char* utf8_path) {
    if (!utf8_path) return IC_FMT_UNKNOWN;
    return (ic_format)ic::detect_format_utf8(utf8_path);
}

IC_API int32_t IC_CALL ic_convert_file(
    const char* utf8_in_path,
    const char* utf8_out_path,
    ic_format target,
    const ic_options* opts,
    char* err_buf,
    int32_t err_buf_size)
{
    if (!utf8_in_path || !utf8_out_path) {
        copy_err(err_buf, err_buf_size, "null path");
        return IC_ERR_INVALID;
    }

    int src = ic::detect_format_utf8(utf8_in_path);
    if (src == IC_FMT_UNKNOWN) {
        copy_err(err_buf, err_buf_size, "unknown input format");
        return IC_ERR_UNSUPPORTED;
    }

    ic::Image img;
    std::string err;
    if (!decode_any(utf8_in_path, src, img, err)) {
        copy_err(err_buf, err_buf_size, err);
        return IC_ERR_DECODE;
    }

    auto opt = to_internal(opts);
    if (!encode_any(utf8_out_path, target, img, opt, err)) {
        copy_err(err_buf, err_buf_size, err);
        return IC_ERR_ENCODE;
    }
    return IC_OK;
}

IC_API const char* IC_CALL ic_version(void) {
    return k_version;
}

IC_API const char* IC_CALL ic_status_string(int32_t status) {
    switch (status) {
        case IC_OK:              return "ok";
        case IC_ERR_OPEN:        return "open failed";
        case IC_ERR_DECODE:      return "decode failed";
        case IC_ERR_ENCODE:      return "encode failed";
        case IC_ERR_UNSUPPORTED: return "format not supported";
        case IC_ERR_IO:          return "i/o error";
        case IC_ERR_OOM:         return "out of memory";
        case IC_ERR_INVALID:     return "invalid argument";
        default:                 return "unknown error";
    }
}

} // extern "C"
