#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oic {

enum class ImageFormat {
    Heif,
    Jpeg,
    Png,
    Webp,
    Tiff,
};

std::wstring toDisplayName(ImageFormat format);
std::wstring defaultExtension(ImageFormat format);
std::vector<ImageFormat> supportedFormats();
std::optional<ImageFormat> formatFromExtension(std::wstring_view extensionOrPath);
std::filesystem::path outputPathFor(const std::filesystem::path& input,
                                    const std::filesystem::path& outputDirectory,
                                    ImageFormat targetFormat);

} // namespace oic
