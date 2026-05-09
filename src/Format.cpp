#include "Format.h"

#include <algorithm>
#include <cwctype>

namespace oic {
namespace {
std::wstring lower(std::wstring_view text) {
    std::wstring result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return result;
}
} // namespace

std::wstring toDisplayName(ImageFormat format) {
    switch (format) {
    case ImageFormat::Heif:
        return L"HEIF / HEIC";
    case ImageFormat::Jpeg:
        return L"JPEG";
    case ImageFormat::Png:
        return L"PNG";
    case ImageFormat::Webp:
        return L"WEBP";
    case ImageFormat::Tiff:
        return L"TIFF";
    }
    return L"Unknown";
}

std::wstring defaultExtension(ImageFormat format) {
    switch (format) {
    case ImageFormat::Heif:
        return L".heic";
    case ImageFormat::Jpeg:
        return L".jpg";
    case ImageFormat::Png:
        return L".png";
    case ImageFormat::Webp:
        return L".webp";
    case ImageFormat::Tiff:
        return L".tiff";
    }
    return L"";
}

std::vector<ImageFormat> supportedFormats() {
    return {ImageFormat::Heif, ImageFormat::Jpeg, ImageFormat::Png, ImageFormat::Webp, ImageFormat::Tiff};
}

std::optional<ImageFormat> formatFromExtension(std::wstring_view extensionOrPath) {
    std::filesystem::path path(extensionOrPath);
    std::wstring ext = path.has_extension() ? path.extension().wstring() : std::wstring(extensionOrPath);
    ext = lower(ext);
    if (!ext.empty() && ext.front() != L'.') {
        ext.insert(ext.begin(), L'.');
    }

    if (ext == L".heif" || ext == L".heic" || ext == L".hif") {
        return ImageFormat::Heif;
    }
    if (ext == L".jpg" || ext == L".jpeg" || ext == L".jpe") {
        return ImageFormat::Jpeg;
    }
    if (ext == L".png") {
        return ImageFormat::Png;
    }
    if (ext == L".webp") {
        return ImageFormat::Webp;
    }
    if (ext == L".tif" || ext == L".tiff") {
        return ImageFormat::Tiff;
    }
    return std::nullopt;
}

std::filesystem::path outputPathFor(const std::filesystem::path& input,
                                    const std::filesystem::path& outputDirectory,
                                    ImageFormat targetFormat) {
    std::filesystem::path output = outputDirectory.empty() ? input.parent_path() : outputDirectory;
    auto stem = input.stem().wstring();
    if (stem.empty()) {
        stem = L"converted";
    }
    output /= stem + defaultExtension(targetFormat);
    return output;
}

} // namespace oic
