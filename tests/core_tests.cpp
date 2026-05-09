#include "ConversionJob.h"

#include <cassert>
#include <filesystem>

int main() {
    using namespace oic;

    assert(formatFromExtension(L"photo.heic") == ImageFormat::Heif);
    assert(formatFromExtension(L"photo.HEIF") == ImageFormat::Heif);
    assert(formatFromExtension(L"photo.jpeg") == ImageFormat::Jpeg);
    assert(formatFromExtension(L"photo.png") == ImageFormat::Png);
    assert(formatFromExtension(L"photo.webp") == ImageFormat::Webp);
    assert(formatFromExtension(L"photo.tif") == ImageFormat::Tiff);
    assert(!formatFromExtension(L"photo.gif"));

    ConversionOptions options;
    options.targetFormat = ImageFormat::Webp;
    options.outputDirectory = L"out";
    auto plan = buildPlan({L"a.jpg", L"b.png", L"c.gif"}, options);
    assert(plan.items.size() == 2);
    assert(plan.skipped.size() == 1);
    assert(plan.items[0].outputPath == std::filesystem::path(L"out/a.webp"));

    return 0;
}
