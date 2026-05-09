#pragma once

#include "ConversionJob.h"

#include <functional>
#include <string>

namespace oic {

struct ConversionProgress {
    std::size_t completed = 0;
    std::size_t total = 0;
    std::wstring currentFile;
};

class WicImageConverter {
public:
    using ProgressCallback = std::function<void(const ConversionProgress&)>;

    WicImageConverter();
    ~WicImageConverter();

    WicImageConverter(const WicImageConverter&) = delete;
    WicImageConverter& operator=(const WicImageConverter&) = delete;

    void convertBatch(const ConversionPlan& plan, const ConversionOptions& options, const ProgressCallback& onProgress);
    bool canEncode(ImageFormat format) const;
    bool canDecode(ImageFormat format) const;

private:
    void* factory_ = nullptr;
};

} // namespace oic
