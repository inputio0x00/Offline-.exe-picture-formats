#include "image.h"

#include <csetjmp>
#include <cstdio>
#include <cstring>

#include <png.h>

namespace ic {

namespace {

void png_user_error(png_structp png, png_const_charp msg) {
    auto* err = reinterpret_cast<std::string*>(png_get_error_ptr(png));
    if (err) *err = msg ? msg : "unknown png error";
    png_longjmp(png, 1);
}

void png_user_warn(png_structp, png_const_charp) {
    // suppress
}

} // namespace

bool decode_png(const std::string& path, Image& out, std::string& err) {
    FILE* fp = fopen_utf8(path, "rb");
    if (!fp) { err = "cannot open file"; return false; }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             &err, png_user_error, png_user_warn);
    if (!png) { std::fclose(fp); err = "png_create_read_struct failed"; return false; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); std::fclose(fp);
                 err = "png_create_info_struct failed"; return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 w = 0, h = 0;
    int bit_depth = 0, color_type = 0;
    png_get_IHDR(png, info, &w, &h, &bit_depth, &color_type, nullptr, nullptr, nullptr);

    // Normalize to 8-bit RGBA.
    if (color_type == PNG_COLOR_TYPE_PALETTE)        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))     png_set_tRNS_to_alpha(png);
    if (bit_depth == 16)                             png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)     png_set_gray_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    out.width = (int)w;
    out.height = (int)h;
    out.has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) ||
                    png_get_valid(png, info, PNG_INFO_tRNS);
    out.pixels.assign(out.stride() * (size_t)out.height, 0);

    std::vector<png_bytep> rows((size_t)h);
    for (size_t y = 0; y < h; ++y) {
        rows[y] = out.pixels.data() + y * out.stride();
    }
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);

    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return true;
}

bool encode_png(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err) {
    if (!img.valid()) { err = "empty image"; return false; }

    FILE* fp = fopen_utf8(path, "wb");
    if (!fp) { err = "cannot create output"; return false; }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              &err, png_user_error, png_user_warn);
    if (!png) { std::fclose(fp); err = "png_create_write_struct failed"; return false; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, nullptr); std::fclose(fp);
                 err = "png_create_info_struct failed"; return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        return false;
    }

    png_init_io(png, fp);

    int level = o.png_compression;
    if (level < 0) level = 0; else if (level > 9) level = 9;
    png_set_compression_level(png, level);

    int color_type = img.has_alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png, info,
                 (png_uint_32)img.width, (png_uint_32)img.height,
                 8, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    if (!img.has_alpha) {
        // Tell libpng that our buffer has a filler byte we want stripped.
        png_set_filler(png, 0, PNG_FILLER_AFTER);
    }

    std::vector<png_bytep> rows((size_t)img.height);
    // const_cast is safe: libpng will not modify the rows when writing.
    auto* base = const_cast<uint8_t*>(img.pixels.data());
    for (int y = 0; y < img.height; ++y) {
        rows[(size_t)y] = base + (size_t)y * img.stride();
    }
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
    return true;
}

} // namespace ic
