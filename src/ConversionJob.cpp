#include "ConversionJob.h"

#include <set>

namespace oic {

ConversionPlan buildPlan(const std::vector<std::filesystem::path>& inputs, const ConversionOptions& options) {
    ConversionPlan plan;
    std::set<std::filesystem::path> seenOutputs;

    for (const auto& input : inputs) {
        if (!formatFromExtension(input.wstring())) {
            plan.skipped.push_back(input.wstring() + L" is not one of HEIF, JPG, PNG, WEBP, or TIFF.");
            continue;
        }

        auto output = outputPathFor(input, options.outputDirectory, options.targetFormat);
        if (!options.overwriteExisting) {
            std::filesystem::path candidate = output;
            int suffix = 1;
            while (seenOutputs.contains(candidate) || std::filesystem::exists(candidate)) {
                candidate = output.parent_path() / (output.stem().wstring() + L" (" + std::to_wstring(suffix++) + L")" + output.extension().wstring());
            }
            output = candidate;
        }
        seenOutputs.insert(output);
        plan.items.push_back({input, output});
    }

    return plan;
}

} // namespace oic
