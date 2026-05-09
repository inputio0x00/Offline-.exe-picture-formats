#pragma once

#include "Format.h"

#include <filesystem>
#include <string>
#include <vector>

namespace oic {

struct ConversionOptions {
    ImageFormat targetFormat = ImageFormat::Png;
    std::filesystem::path outputDirectory;
    int quality = 90;
    bool losslessWebp = false;
    bool preserveMetadata = true;
    bool overwriteExisting = false;
};

struct ConversionItem {
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
};

struct ConversionPlan {
    std::vector<ConversionItem> items;
    std::vector<std::wstring> skipped;
};

ConversionPlan buildPlan(const std::vector<std::filesystem::path>& inputs, const ConversionOptions& options);

} // namespace oic
