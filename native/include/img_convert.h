// img_convert.h - C ABI for the native image conversion core.
//
// All strings are UTF-8 unless otherwise stated. Paths on Windows are
// passed as UTF-8 and converted internally to wide strings; the C# side
// uses Encoding.UTF8 in its P/Invoke marshalling.
#ifndef IMG_CONVERT_H
#define IMG_CONVERT_H

#include <stdint.h>

#if defined(_WIN32)
  #if defined(IMG_CONVERT_BUILD_DLL)
    #define IC_API __declspec(dllexport)
  #else
    #define IC_API __declspec(dllimport)
  #endif
  #define IC_CALL __cdecl
#else
  #define IC_API __attribute__((visibility("default")))
  #define IC_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ic_format {
    IC_FMT_UNKNOWN = 0,
    IC_FMT_JPG     = 1,
    IC_FMT_PNG     = 2,
    IC_FMT_WEBP    = 3,
    IC_FMT_TIFF    = 4,
    IC_FMT_HEIF    = 5
} ic_format;

typedef enum ic_tiff_compression {
    IC_TIFF_NONE = 0,
    IC_TIFF_LZW  = 1,
    IC_TIFF_ZIP  = 2,
    IC_TIFF_PACKBITS = 3
} ic_tiff_compression;

typedef enum ic_status {
    IC_OK              = 0,
    IC_ERR_OPEN        = -1,
    IC_ERR_DECODE      = -2,
    IC_ERR_ENCODE      = -3,
    IC_ERR_UNSUPPORTED = -4,
    IC_ERR_IO          = -5,
    IC_ERR_OOM         = -6,
    IC_ERR_INVALID     = -7
} ic_status;

// All optional fields use sentinel values described per-field. A zero-
// initialized struct is a sane default for every format.
typedef struct ic_options {
    // 1-100. Used by JPG, WEBP (lossy), HEIF. 0 means "use default" (90).
    int32_t quality;

    // 0-9. PNG zlib compression level. 0 means "use default" (6).
    int32_t png_compression;

    // Non-zero = lossless mode for WEBP/HEIF. PNG/TIFF(*) are always lossless.
    int32_t lossless;

    // Non-zero = JPEG progressive output.
    int32_t jpeg_progressive;

    // ic_tiff_compression value. 0 = no compression, 1 = LZW (default if 0
    // is passed and the source had alpha or 16-bit), etc.
    int32_t tiff_compression;

    // Reserved for future use; pass 0.
    int32_t reserved[6];
} ic_options;

// Detect format by reading the first few bytes of the file at utf8_path.
// Returns IC_FMT_UNKNOWN if unrecognized or unreadable.
IC_API ic_format IC_CALL ic_detect_format(const char* utf8_path);

// Convert a single file from any supported format to `target`.
// `err_buf` (may be NULL) receives a UTF-8 NUL-terminated message on error.
IC_API int32_t IC_CALL ic_convert_file(
    const char* utf8_in_path,
    const char* utf8_out_path,
    ic_format target,
    const ic_options* opts,
    char* err_buf,
    int32_t err_buf_size);

// Library version, e.g. "1.0.0". Caller must NOT free.
IC_API const char* IC_CALL ic_version(void);

// Returns a human-readable string for a status code. Caller must NOT free.
IC_API const char* IC_CALL ic_status_string(int32_t status);

#ifdef __cplusplus
}
#endif

#endif // IMG_CONVERT_H
