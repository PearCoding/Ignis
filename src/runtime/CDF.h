#pragma once

#include "IG_Config.h"

namespace IG {
class CDF {
public:
    static void computeForArray(const std::vector<float>& values, const std::filesystem::path& out);
    static void computeForImage(const std::filesystem::path& in, const std::filesystem::path& out,
                                size_t& slice_conditional, size_t& slice_marginal,
                                bool premultiplySin, bool compensate);
};
} // namespace IG