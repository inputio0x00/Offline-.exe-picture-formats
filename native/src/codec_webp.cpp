#include "image.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#include <webp/decode.h>
#include <webp/encode.h>

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

bool write_whole_file(const std::string& path, const uint8_t* data, size_t size, std::string& err) {
    FILE* fp = fopen_utf8(path, "wb");
    if (!fp) { err = "cannot create output"; return false; }
    size_t n = std::fwrite(data, 1, size, fp);
    std::fclose(fp);
    if (n != size) { err = "short write"; return false; }
    return true;
}

} // namespace

bool decode_webp(const std::string& path, Image& out, std::string& err) {
    std::vector<uint8_t> buf;
    if (!read_whole_file(path, buf, err)) return false;

    WebPBitstreamFeatures feat;
    if (WebPGetFeatures(buf.data(), buf.size(), &feat) != VP8_STATUS_OK) {
        err = "not a WEBP file";
        return false;
    }

    int w = 0, h = 0;
    uint8_t* pixels = WebPDecodeRGBA(buf.data(), buf.size(), &w, &h);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) WebPFree(pixels);
        err = "WebPDecodeRGBA failed";
        return false;
    }

    out.width = w;
    out.height = h;
    out.has_alpha = feat.has_alpha != 0;
    out.pixels.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
    WebPFree(pixels);
    return true;
}

bool encode_webp(const std::string& path, const Image& img, const EncodeOptions& o, std::string& err) {
    if (!img.valid()) { err = "empty image"; return false; }

    uint8_t* output = nullptr;
    size_t out_size = 0;
    int stride = (int)img.stride();

    if (o.lossless) {
        if (img.has_alpha) {
            out_size = WebPEncodeLosslessRGBA(img.pixels.data(), img.width, img.height, stride, &output);
        } else {
            // Pack to RGB to avoid encoding the filler alpha unnecessarily.
            std::vector<uint8_t> rgb((size_t)img.width * (size_t)img.height * 3);
            for (int y = 0; y < img.height; ++y) {
                const uint8_t* s = img.pixels.data() + (size_t)y * img.stride();
                uint8_t* d = rgb.data() + (size_t)y * (size_t)img.width * 3;
                for (int x = 0; x < img.width; ++x) {
                    d[x * 3 + 0] = s[x * 4 + 0];
                    d[x * 3 + 1] = s[x * 4 + 1];
                    d[x * 3 + 2] = s[x * 4 + 2];
                }
            }
            out_size = WebPEncodeLosslessRGB(rgb.data(), img.width, img.height, img.width * 3, &output);
        }
    } else {
        float q = (float)(o.quality > 0 ? o.quality : 90);
        if (q < 1.0f) q = 1.0f; else if (q > 100.0f) q = 100.0f;
        if (img.has_alpha) {
            out_size = WebPEncodeRGBA(img.pixels.data(), img.width, img.height, stride, q, &output);
        } else {
            std::vector<uint8_t> rgb((size_t)img.width * (size_t)img.height * 3);
            for (int y = 0; y < img.height; ++y) {
                const uint8_t* s = img.pixels.data() + (size_t)y * img.stride();
                uint8_t* d = rgb.data() + (size_t)y * (size_t)img.width * 3;
                for (int x = 0; x < img.width; ++x) {
                    d[x * 3 + 0] = s[x * 4 + 0];
                    d[x * 3 + 1] = s[x * 4 + 1];
                    d[x * 3 + 2] = s[x * 4 + 2];
                }
            }
            out_size = WebPEncodeRGB(rgb.data(), img.width, img.height, img.width * 3, q, &output);
        }
    }

    if (out_size == 0 || !output) {
        err = "WEBP encode failed";
        if (output) WebPFree(output);
        return false;
    }

    bool ok = write_whole_file(path, output, out_size, err);
    WebPFree(output);
    return ok;
}

} // namespace ic
