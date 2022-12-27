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

void CDF::computeForImage(const Image& image, const std::filesystem::path& out,
                          size_t& slice_conditional, size_t& slice_marginal,
                          bool premultiplySin, bool compensate)
{
    constexpr float MinEps = 1e-5f;

    std::vector<float> marginal(image.height);
    std::vector<float> conditional(image.width * image.height);

    slice_conditional = image.width;
    slice_marginal    = image.height;

    const size_t c = image.channels;
    IG_ASSERT(c == 3 || c == 4, "Expected cdf image to have four or three channels per pixel");

    // Apply MIS compensation if necessary
    Vector3f defect = Vector3f::Zero();
    if (compensate) {
        for (size_t i = 0; i < image.width * image.height; ++i) {
            defect.x() += std::max(image.pixels[i * c + 0], 0.0f) / image.width;
            defect.y() += std::max(image.pixels[i * c + 1], 0.0f) / image.width;
            defect.z() += std::max(image.pixels[i * c + 2], 0.0f) / image.width;
        }
        defect /= (float)image.height; // We split width & height to prevent large divisions
    }

    // Compute per pixel average over image
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, image.height),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t y = range.begin(); y < range.end(); ++y) {
                const float* p = &image.pixels[y * image.width * c];
                float* cond    = &conditional[y * image.width];

                // Compute one dimensional cdf per row
                cond[0] = (std::max(p[0] - defect.x(), 0.0f)
                           + std::max(p[1] - defect.y(), 0.0f)
                           + std::max(p[2] - defect.z(), 0.0f))
                          / 3;
                for (size_t x = 1; x < image.width; ++x)
                    cond[x] = cond[x - 1]
                              + (std::max(p[x * c + 0] - defect.x(), 0.0f)
                                 + std::max(p[x * c + 1] - defect.y(), 0.0f)
                                 + std::max(p[x * c + 2] - defect.z(), 0.0f))
                                    / 3;
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

void CDF::computeForImageSAT(const Image& image, const std::filesystem::path& out,
                             size_t& size, size_t& slice, bool premultiplySin, bool compensate)
{
    constexpr float MinEps = 1e-5f;

    std::vector<float> data(image.width * image.height);
    size  = data.size();
    slice = image.width;

    const size_t c = image.channels;
    IG_ASSERT(c == 3 || c == 4, "Expected cdf image to have four or three channels per pixel");

    // Apply MIS compensation if necessary
    Vector3f defect = Vector3f::Zero();
    if (compensate) {
        for (size_t i = 0; i < image.width * image.height; ++i) {
            defect.x() += std::max(image.pixels[i * c + 0], 0.0f) / image.width;
            defect.y() += std::max(image.pixels[i * c + 1], 0.0f) / image.width;
            defect.z() += std::max(image.pixels[i * c + 2], 0.0f) / image.width;
        }
        defect /= (float)image.height; // We split width & height to prevent large divisions
    }

    // Compute per pixel average over image
    for (size_t y = 0; y < image.height; ++y) {
        const float factor = premultiplySin ? std::sin(Pi * (y + 0.5f) / float(image.height)) : 1.0f;
        for (size_t x = 0; x < image.width; ++x) {
            const size_t id = y * image.width + x;
            const float* p  = &image.pixels[id * c];
            const float val = (factor / 3) * (std::max(p[0] - defect.x(), 0.0f) + std::max(p[1] - defect.y(), 0.0f) + std::max(p[2] - defect.z(), 0.0f));

            const float vxpy  = y > 0 ? data[id - image.width] : 0.0f;
            const float vpxy  = x > 0 ? data[id - 1] : 0.0f;
            const float vpxpy = x > 0 && y > 0 ? data[id - image.width - 1] : 0.0f;

            data[y * image.width + x] = val + vxpy + vpxy - vpxpy;
        }
    }

    // Normalize data
    const float sum = data.back();
    if (sum > MinEps) {
        const float n = 1.0f / sum;
        for (float& f : data)
            f *= n;
    } else {
        const float n = 1.0f / (data.size() - 1);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = i * n;
    }

    // Force 1 to make it numerically stable
    data.back() = 1;

    // Write data to file
    FileSerializer serializer(out, false);
    serializer.write(data, true);
}

} // namespace IG