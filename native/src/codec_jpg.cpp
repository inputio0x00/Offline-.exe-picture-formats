#include "image.h"

#include <csetjmp>
#include <cstdio>
#include <cstring>

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

namespace ic {

namespace {

struct jpeg_err_mgr {
    jpeg_error_mgr base;
    jmp_buf jmp;
    std::string msg;
};

void on_jpeg_error(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<jpeg_err_mgr*>(cinfo->err);
    char buf[JMSG_LENGTH_MAX] = {0};
    (*cinfo->err->format_message)(cinfo, buf);
    err->msg = buf;
    std::longjmp(err->jmp, 1);
}

void on_jpeg_emit(j_common_ptr, int) {
    // suppress libjpeg's stderr warnings
}

} // namespace

bool decode_jpg(const std::string& path, Image& out, std::string& err) {
    FILE* fp = fopen_utf8(path, "rb");
    if (!fp) { err = "cannot open file"; return false; }

    jpeg_decompress_struct cinfo;
    jpeg_err_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr.base);
    jerr.base.error_exit = on_jpeg_error;
    jerr.base.emit_message = on_jpeg_emit;

    if (setjmp(jerr.jmp)) {
        jpeg_destroy_decompress(&cinfo);
        std::fclose(fp);
        err = "jpeg decode: " + jerr.msg;
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    out.width  = (int)cinfo.output_width;
    out.height = (int)cinfo.output_height;
    out.has_alpha = false;
    out.pixels.assign(out.stride() * (size_t)out.height, 0xFF);

    std::vector<uint8_t> row((size_t)cinfo.output_width * 3);
    JSAMPROW row_ptr = row.data();

    while (cinfo.output_scanline < cinfo.output_height) {
        int y = (int)cinfo.output_scanline;
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
        uint8_t* dst = out.pixels.data() + (size_t)y * out.stride();
        for (int x = 0; x < out.width; ++x) {
            dst[x * 4 + 0] = row[x * 3 + 0];
            dst[x * 4 + 1] = row[x * 3 + 1];
            dst[x * 4 + 2] = row[x * 3 + 2];
            dst[x * 4 + 3] = 0xFF;
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    std::fclose(fp);
    return true;
}

bool encode_jpg(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err) {
    if (!img.valid()) { err = "empty image"; return false; }

    FILE* fp = fopen_utf8(path, "wb");
    if (!fp) { err = "cannot create output"; return false; }

    jpeg_compress_struct cinfo;
    jpeg_err_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr.base);
    jerr.base.error_exit = on_jpeg_error;
    jerr.base.emit_message = on_jpeg_emit;

    if (setjmp(jerr.jmp)) {
        jpeg_destroy_compress(&cinfo);
        std::fclose(fp);
        err = "jpeg encode: " + jerr.msg;
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = (JDIMENSION)img.width;
    cinfo.image_height = (JDIMENSION)img.height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    int q = o.quality > 0 ? o.quality : 90;
    if (q < 1) q = 1; else if (q > 100) q = 100;
    jpeg_set_quality(&cinfo, q, TRUE);

    if (o.jpeg_progressive) {
        jpeg_simple_progression(&cinfo);
    }

    jpeg_start_compress(&cinfo, TRUE);

    std::vector<uint8_t> row((size_t)img.width * 3);
    while (cinfo.next_scanline < cinfo.image_height) {
        int y = (int)cinfo.next_scanline;
        const uint8_t* src = img.pixels.data() + (size_t)y * img.stride();
        // Composite over white if the source has alpha — JPEG has no alpha.
        for (int x = 0; x < img.width; ++x) {
            uint8_t r = src[x * 4 + 0];
            uint8_t g = src[x * 4 + 1];
            uint8_t b = src[x * 4 + 2];
            uint8_t a = src[x * 4 + 3];
            if (img.has_alpha && a < 255) {
                int inv = 255 - a;
                r = (uint8_t)((r * a + 255 * inv + 127) / 255);
                g = (uint8_t)((g * a + 255 * inv + 127) / 255);
                b = (uint8_t)((b * a + 255 * inv + 127) / 255);
            }
            row[x * 3 + 0] = r;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = b;
        }
        JSAMPROW rp = row.data();
        jpeg_write_scanlines(&cinfo, &rp, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    std::fclose(fp);
    return true;
}

} // namespace ic
