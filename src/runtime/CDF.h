#pragma once

#include "Image.h"

namespace IG {
class CDF {
public:
    static void computeForArray(const std::vector<float>& values, const std::filesystem::path& out);
    static void computeForImage(const Image& image, const std::filesystem::path& out,
                                size_t& slice_conditional, size_t& slice_marginal,
                                bool premultiplySin, bool compensate);
    static void computeForImageSAT(const Image& image, const std::filesystem::path& out,
                                   size_t& size, size_t& slice, bool premultiplySin, bool compensate);
};
} // namespace IG