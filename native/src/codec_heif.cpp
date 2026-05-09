#include "image.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <libheif/heif.h>

namespace ic {

namespace {

bool read_whole_file(const std::string& path, std::vector<uint8_t>& buf, std::string& err) {
    FILE* fp = fopen_utf8(path, "rb");
    if (!fp) { err = "cannot open file"; return false; }
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    if (sz < 0) { std::fclose(fp); err = "ftell failed"; return false; }
    std::fseek(fp, 0, SEEK_SET);
    buf.resize((size_t)sz);
    size_t n = std::fread(buf.data(), 1, buf.size(), fp);
    std::fclose(fp);
    if (n != buf.size()) { err = "short read"; return false; }
    return true;
}

bool write_whole_file(const std::string& path, const void* data, size_t size, std::string& err) {
    FILE* fp = fopen_utf8(path, "wb");
    if (!fp) { err = "cannot create output"; return false; }
    size_t n = std::fwrite(data, 1, size, fp);
    std::fclose(fp);
    if (n != size) { err = "short write"; return false; }
    return true;
}

struct HeifWriterCtx {
    std::vector<uint8_t>* sink;
};

heif_error heif_writer_write(struct heif_context* /*ctx*/, const void* data, size_t size, void* userdata) {
    auto* w = reinterpret_cast<HeifWriterCtx*>(userdata);
    auto* bytes = reinterpret_cast<const uint8_t*>(data);
    w->sink->insert(w->sink->end(), bytes, bytes + size);
    heif_error e{ heif_error_Ok, heif_suberror_Unspecified, "" };
    return e;
}

} // namespace

bool decode_heif(const std::string& path, Image& out, std::string& err) {
    std::vector<uint8_t> buf;
    if (!read_whole_file(path, buf, err)) return false;

    heif_context* ctx = heif_context_alloc();
    if (!ctx) { err = "heif_context_alloc failed"; return false; }

    heif_error e = heif_context_read_from_memory_without_copy(ctx, buf.data(), buf.size(), nullptr);
    if (e.code != heif_error_Ok) {
        err = std::string("heif read: ") + (e.message ? e.message : "");
        heif_context_free(ctx);
        return false;
    }

    heif_image_handle* handle = nullptr;
    e = heif_context_get_primary_image_handle(ctx, &handle);
    if (e.code != heif_error_Ok || !handle) {
        err = std::string("heif primary handle: ") + (e.message ? e.message : "");
        heif_context_free(ctx);
        return false;
    }

    bool has_alpha = heif_image_handle_has_alpha_channel(handle) != 0;

    heif_image* img = nullptr;
    e = heif_decode_image(handle, &img,
                          heif_colorspace_RGB,
                          has_alpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB,
                          nullptr);
    if (e.code != heif_error_Ok || !img) {
        err = std::string("heif decode: ") + (e.message ? e.message : "");
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return false;
    }

    int w = heif_image_get_width(img, heif_channel_interleaved);
    int h = heif_image_get_height(img, heif_channel_interleaved);
    int stride = 0;
    const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
    if (!data || w <= 0 || h <= 0) {
        err = "heif: empty plane";
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return false;
    }

    out.width = w;
    out.height = h;
    out.has_alpha = has_alpha;
    out.pixels.assign((size_t)w * (size_t)h * 4, 0xFF);

    int src_pixel_bytes = has_alpha ? 4 : 3;
    for (int y = 0; y < h; ++y) {
        const uint8_t* s = data + (size_t)y * (size_t)stride;
        uint8_t* d = out.pixels.data() + (size_t)y * out.stride();
        if (has_alpha) {
            std::memcpy(d, s, (size_t)w * 4);
        } else {
            for (int x = 0; x < w; ++x) {
                d[x * 4 + 0] = s[x * src_pixel_bytes + 0];
                d[x * 4 + 1] = s[x * src_pixel_bytes + 1];
                d[x * 4 + 2] = s[x * src_pixel_bytes + 2];
                d[x * 4 + 3] = 0xFF;
            }
        }
    }

    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    return true;
}

bool encode_heif(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err) {
    if (!img.valid()) { err = "empty image"; return false; }

    heif_context* ctx = heif_context_alloc();
    if (!ctx) { err = "heif_context_alloc failed"; return false; }

    heif_image* himg = nullptr;
    heif_error e = heif_image_create(img.width, img.height,
                                     heif_colorspace_RGB,
                                     img.has_alpha ? heif_chroma_interleaved_RGBA
                                                   : heif_chroma_interleaved_RGB,
                                     &himg);
    if (e.code != heif_error_Ok || !himg) {
        err = std::string("heif_image_create: ") + (e.message ? e.message : "");
        heif_context_free(ctx);
        return false;
    }

    e = heif_image_add_plane(himg, heif_channel_interleaved, img.width, img.height, 8);
    if (e.code != heif_error_Ok) {
        err = std::string("heif_image_add_plane: ") + (e.message ? e.message : "");
        heif_image_release(himg);
        heif_context_free(ctx);
        return false;
    }

    int dst_stride = 0;
    uint8_t* dst = heif_image_get_plane(himg, heif_channel_interleaved, &dst_stride);
    int dst_pixel_bytes = img.has_alpha ? 4 : 3;
    for (int y = 0; y < img.height; ++y) {
        const uint8_t* s = img.pixels.data() + (size_t)y * img.stride();
        uint8_t* d = dst + (size_t)y * (size_t)dst_stride;
        if (img.has_alpha) {
            std::memcpy(d, s, (size_t)img.width * 4);
        } else {
            for (int x = 0; x < img.width; ++x) {
                d[x * dst_pixel_bytes + 0] = s[x * 4 + 0];
                d[x * dst_pixel_bytes + 1] = s[x * 4 + 1];
                d[x * dst_pixel_bytes + 2] = s[x * 4 + 2];
            }
        }
    }

    heif_encoder* enc = nullptr;
    e = heif_context_get_encoder_for_format(ctx, heif_compression_HEVC, &enc);
    if (e.code != heif_error_Ok || !enc) {
        err = std::string("heif encoder: ") + (e.message ? e.message : "no HEVC encoder");
        heif_image_release(himg);
        heif_context_free(ctx);
        return false;
    }

    if (o.lossless) {
        heif_encoder_set_lossless(enc, 1);
        heif_encoder_set_parameter_string(enc, "chroma", "444");
    } else {
        heif_encoder_set_lossless(enc, 0);
        int q = o.quality > 0 ? o.quality : 90;
        if (q < 1) q = 1; else if (q > 100) q = 100;
        heif_encoder_set_lossy_quality(enc, q);
    }

    heif_encoding_options* enc_opts = heif_encoding_options_alloc();
    e = heif_context_encode_image(ctx, himg, enc, enc_opts, nullptr);
    heif_encoding_options_free(enc_opts);
    heif_encoder_release(enc);
    heif_image_release(himg);

    if (e.code != heif_error_Ok) {
        err = std::string("heif encode: ") + (e.message ? e.message : "");
        heif_context_free(ctx);
        return false;
    }

    std::vector<uint8_t> sink;
    HeifWriterCtx wctx{ &sink };
    heif_writer writer{};
    writer.writer_api_version = 1;
    writer.write = heif_writer_write;
    e = heif_context_write(ctx, &writer, &wctx);
    heif_context_free(ctx);
    if (e.code != heif_error_Ok) {
        err = std::string("heif write: ") + (e.message ? e.message : "");
        return false;
    }

    return write_whole_file(path, sink.data(), sink.size(), err);
}

} // namespace ic
