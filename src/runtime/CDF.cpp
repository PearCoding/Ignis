#include "CDF.h"
#include "Image.h"
#include "serialization/FileSerializer.h"

IG_BEGIN_IGNORE_WARNINGS
#include <tbb/parallel_for.h>
IG_END_IGNORE_WARNINGS

namespace IG {

void CDF::computeForArray(const std::vector<float>& values, const std::filesystem::path& out)
{
    constexpr float MinEps = 1e-5f;

    std::vector<float> cdf(values.size());

    // Do the thing
    cdf[0] = values[0];
    for (size_t x = 1; x < cdf.size(); ++x)
        cdf[x] = cdf[x - 1] + values[x];
    const float sum = cdf.back();

    // Normalize row
    if (sum > MinEps) {
        const float n = 1.0f / sum;
        for (float& v : cdf)
            v *= n;
    } else {
        const float n = 1.0f / values.size();
        for (size_t x = 0; x < values.size(); ++x)
            cdf[x] = x * n;
    }

    // Force 1 to make it numerically stable
    cdf.back() = 1;

    // Write data
    FileSerializer serializer(out, false);
    serializer.write(cdf, true);
}

void CDF::computeForImage(const std::filesystem::path& in, const std::filesystem::path& out,
                          size_t& slice_conditional, size_t& slice_marginal,
                          bool premultiplySin)
{
    constexpr float MinEps = 1e-5f;

    Image image = Image::load(in);

    std::vector<float> marginal(image.height);
    std::vector<float> conditional(image.width * image.height);

    slice_conditional = image.width;
    slice_marginal    = image.height;

    const size_t c = image.channels;
    IG_ASSERT(c == 3 || c == 4, "Expected cdf image to have four or three channels per pixel");

    // Compute per pixel average over image
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, image.height),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t y = range.begin(); y < range.end(); ++y) {
                const float* p = &image.pixels[y * image.width * c];
                float* cond    = &conditional[y * image.width];

                // Compute one dimensional cdf per row
                cond[0] = (p[0] + p[1] + p[2]) / 3;
                for (size_t x = 1; x < image.width; ++x)
                    cond[x] = cond[x - 1] + (p[x * c + 0] + p[x * c + 1] + p[x * c + 2]) / 3;
                const float sum = cond[image.width - 1];

                // Set as marginal
                if (premultiplySin)
                    marginal[y] = sum * std::sin(Pi * (y + 0.5f) / float(image.height));
                else
                    marginal[y] = sum;

                // Normalize row
                if (sum > MinEps) {
                    const float n = 1.0f / sum;
                    for (size_t x = 0; x < image.width; ++x)
                        cond[x] *= n;
                } else {
                    const float n = 1.0f / (image.width - 1);
                    for (size_t x = 0; x < image.width; ++x)
                        cond[x] = x * n;
                }

                // Force 1 to make it numerically stable
                cond[image.width - 1] = 1;
            }
        });

    // Sum marginal
    for (size_t y = 1; y < image.height; ++y)
        marginal[y] += marginal[y - 1];

    // Normalize marginal
    if (marginal[image.height - 1] > MinEps) {
        const float n = 1.0f / marginal[image.height - 1];
        for (float& e : marginal)
            e *= n;
    } else {
        const float n = 1.0f / (image.height - 1);
        for (size_t y = 0; y < image.height; ++y)
            marginal[y] = y * n;
    }

    // Force 1 to make it numerically stable
    marginal[image.height - 1] = 1;

    // Write data to file
    FileSerializer serializer(out, false);
    serializer.write(marginal, true);
    serializer.write(conditional, true);
}
} // namespace IG