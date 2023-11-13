#include "CDF.h"
#include "Image.h"
#include "Logger.h"
#include "serialization/FileSerializer.h"

IG_BEGIN_IGNORE_WARNINGS
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
IG_END_IGNORE_WARNINGS

namespace IG {

void CDF::computeForArray(const std::vector<float>& values, const Path& out)
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
        // Skip last entry as it is explicty set to 1
        const float n = 1.0f / values.size();
        for (size_t x = 1; x < values.size(); ++x)
            cdf[x - 1] = x * n;
    }

    // Force 1 to make it numerically stable
    cdf.back() = 1;

    // Write data
    FileSerializer serializer(out, false);
    serializer.write(cdf, true);
}

static inline float colorResponse(float r, float g, float b)
{
    return (std::max(r, 0.0f) + std::max(g, 0.0f) + std::max(b, 0.0f)) / 3;
}

static inline float computeMISDefect(const Image& image)
{
    const size_t c = image.channels;

    float defect   = 0;
    float minValue = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < image.width * image.height; ++i) {
        const float response = colorResponse(image.pixels[i * c + 0], image.pixels[i * c + 1], image.pixels[i * c + 2]);
        minValue             = std::min(minValue, response);
        defect += response / image.width;
    }
    defect /= (float)image.height; // We split width & height to prevent large divisions

    // If image is constant color, do not apply MIS compensation
    if (std::abs(minValue - defect) < 1e-4f)
        return 0;
    else
        return defect;
}

void CDF::computeForImage(const Image& image, const Path& out,
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
    float defect = 0;
    if (compensate)
        defect = computeMISDefect(image);

    // Compute per pixel average over image
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, image.height),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t y = range.begin(); y < range.end(); ++y) {
                const float* p = &image.pixels[y * image.width * c];
                float* cond    = &conditional[y * image.width];

                // Compute one dimensional cdf per row
                cond[0] = colorResponse(p[0] - defect, p[1] - defect, p[2] - defect);
                for (size_t x = 1; x < image.width; ++x)
                    cond[x] = cond[x - 1] + colorResponse(p[x * c + 0] - defect, p[x * c + 1] - defect, p[x * c + 2] - defect);
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
                    // Skip last entry as it is explicty set to 1
                    const float n = 1.0f / image.width;
                    for (size_t x = 1; x < image.width; ++x)
                        cond[x - 1] = x * n;
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
        // Skip last entry as it is explicty set to 1
        const float n = 1.0f / image.height;
        for (size_t y = 1; y < image.height; ++y)
            marginal[y - 1] = y * n;
    }

    // Force 1 to make it numerically stable
    marginal[image.height - 1] = 1;

    // Write data to file
    FileSerializer serializer(out, false);
    serializer.write(marginal, true);
    serializer.write(conditional, true);
}

void CDF::computeForImageSAT(const Image& image, const Path& out,
                             size_t& size, size_t& width, size_t& height, bool premultiplySin, bool compensate)
{
    constexpr float MinEps = 1e-5f;

    size   = image.width * image.height;
    width  = image.width;
    height = image.height;

    const size_t c = image.channels;
    IG_ASSERT(c == 3 || c == 4, "Expected cdf image to have four or three channels per pixel");

    // Apply MIS compensation if necessary
    float defect = 0;
    if (compensate)
        defect = computeMISDefect(image);

    // Compute per pixel average over image
    std::vector<float> pdf(image.width * image.height);
    for (size_t y = 0; y < image.height; ++y) {
        const float factor = premultiplySin ? std::sin(Pi * (y + 0.5f) / float(image.height)) : 1.0f;
        for (size_t x = 0; x < image.width; ++x) {
            const size_t id = y * image.width + x;
            const float* p  = &image.pixels[id * c];
            const float val = factor * colorResponse(p[0] - defect, p[1] - defect, p[2] - defect);
            pdf[id]         = val;
        }
    }

    // Compute sum table
    std::vector<float> sat(image.width * image.height);
    for (size_t y = 0; y < image.height; ++y) {
        for (size_t x = 0; x < image.width; ++x) {
            const size_t id = y * image.width + x;
            const float val = pdf[id];

            const float vxpy  = y > 0 ? sat[id - image.width] : 0.0f;
            const float vpxy  = x > 0 ? sat[id - 1] : 0.0f;
            const float vpxpy = x > 0 && y > 0 ? sat[id - image.width - 1] : 0.0f;

            sat[y * image.width + x] = val + vxpy + vpxy - vpxpy;
        }
    }

    // Normalize data
    const float sum = sat.back();
    if (sum > MinEps) {
        const float n = 1.0f / sum;
        for (float& f : sat)
            f *= n;
    } else {
        const float n = 1.0f / (sat.size() - 1);
        for (size_t i = 0; i < sat.size(); ++i)
            sat[i] = i * n;
    }

    // Force 1 to make it numerically stable
    sat.back() = 1;

    // Write data to file
    FileSerializer serializer(out, false);
    serializer.write(sat, true);
    serializer.write(pdf, true); // Keep the pdf
}

// Traverse bottom to top to create mip map
static std::vector<float> computeMipMap(const std::vector<float>& detailed, size_t sliceDetailed)
{
    if (sliceDetailed <= 2)
        return {};

    const size_t sliceCoarse = sliceDetailed / 2;

    std::vector<float> out(sliceCoarse * sliceCoarse);

    tbb::parallel_for(
        tbb::blocked_range2d<size_t>(0, sliceCoarse, 0, sliceCoarse),
        [&](const tbb::blocked_range2d<size_t>& range) {
            for (size_t y = range.rows().begin(); y < range.rows().end(); ++y) {
                for (size_t x = range.cols().begin(); x < range.cols().end(); ++x) {
                    const bool hasRight  = 2 * x + 1 < sliceDetailed;
                    const bool hasBottom = 2 * y + 1 < sliceDetailed;

                    const int elems = 2 + 2 * (int)hasBottom;
                    const float x00 = detailed[(2 * y + 0) * sliceDetailed + 2 * x + 0];
                    const float x01 = hasRight ? detailed[(2 * y + 0) * sliceDetailed + 2 * x + 1] : detailed[(2 * y + 0) * sliceDetailed + 0]; // Repeat on the x-axis
                    const float x10 = hasBottom ? detailed[(2 * y + 1) * sliceDetailed + 2 * x + 0] : 0.0f;
                    const float x11 = hasRight && hasBottom ? detailed[(2 * y + 1) * sliceDetailed + 2 * x + 1] : (hasBottom ? detailed[(2 * y + 1) * sliceDetailed + 0] : 0.0f);

                    out[y * sliceCoarse + x] = (x00 + x01 + x10 + x11) / elems;
                }
            }
        });

    return out;
}

// TODO: These two downsamplers are horrible, but should get the job done

static std::vector<float> downsampleX(const std::vector<float>& in, size_t width, size_t height, size_t new_width)
{
    if (width == new_width)
        return in;

    std::vector<float> out(new_width * height);

    const size_t sliceIn  = width;
    const size_t sliceOut = new_width;

    // const float scalefactor = width / (float)new_width;

    tbb::parallel_for(
        tbb::blocked_range2d<size_t>(0, height, 0, new_width),
        [&](const tbb::blocked_range2d<size_t>& range) {
            for (size_t y = range.rows().begin(); y < range.rows().end(); ++y) {
                for (size_t x = range.cols().begin(); x < range.cols().end(); ++x) {
                    const size_t bx = std::min((size_t)std::ceil((x + 0) / (float)(new_width - 1) * width), width - 1);
                    const size_t ex = std::min((size_t)std::ceil((x + 1) / (float)(new_width - 1) * width), width - 1);

                    // TODO: Better filter
                    if (bx == ex) {
                        out[y * sliceOut + x] = in[y * sliceIn + bx]; // Same
                    } else {
                        float data = 0;
                        for (size_t ix = bx; ix < ex; ++ix)
                            data += in[y * sliceIn + ix];

                        out[y * sliceOut + x] = data / (ex - bx);
                    }
                }
            }
        });

    return out;
}

static std::vector<float> downsampleY(const std::vector<float>& in, size_t width, size_t height, size_t new_height)
{
    if (height == new_height)
        return in;

    std::vector<float> out(width * new_height);

    const size_t sliceIn  = width;
    const size_t sliceOut = width;

    // const float scalefactor = height / (float)new_height;

    tbb::parallel_for(
        tbb::blocked_range2d<size_t>(0, new_height, 0, width),
        [&](const tbb::blocked_range2d<size_t>& range) {
            for (size_t y = range.rows().begin(); y < range.rows().end(); ++y) {
                const size_t by = std::min((size_t)std::ceil((y + 0) / (float)(new_height - 1) * height), height - 1);
                const size_t ey = std::min((size_t)std::ceil((y + 1) / (float)(new_height - 1) * height), height - 1);
                for (size_t x = range.cols().begin(); x < range.cols().end(); ++x) {
                    // TODO: Better filter
                    if (by == ey) {
                        out[y * sliceOut + x] = in[by * sliceIn + x]; // Same
                    } else {
                        float data = 0;
                        for (size_t iy = by; iy < ey; ++iy)
                            data += in[iy * sliceIn + x];

                        out[y * sliceOut + x] = data / (ey - by);
                    }
                }
            }
        });

    return out;
}

void CDF::computeForImageHierachical(const Image& image, const Path& out,
                                     size_t& size, size_t& slice, size_t& levels, bool premultiplySin, bool compensate)
{
    const size_t c = image.channels;
    IG_ASSERT(c == 3 || c == 4, "Expected cdf image to have four or three channels per pixel");

    // Apply MIS compensation if necessary
    float defect = 0;
    if (compensate)
        defect = computeMISDefect(image);

    // Compute per pixel average over image
    std::vector<float> initial(image.width * image.height);
    for (size_t y = 0; y < image.height; ++y) {
        const float factor = premultiplySin ? std::sin(Pi * (y + 0.5f) / float(image.height)) : 1.0f;
        for (size_t x = 0; x < image.width; ++x) {
            const size_t id = y * image.width + x;
            const float* p  = &image.pixels[id * c];
            const float val = factor * colorResponse(p[0] - defect, p[1] - defect, p[2] - defect);
            initial[id]     = val;
        }
    }

    // Normalize
    const float sum = tbb::parallel_reduce(
                          tbb::blocked_range<float*>(initial.data(), initial.data() + initial.size()),
                          0.f,
                          [](const tbb::blocked_range<float*>& r, float init) -> float {
                              for (float* a = r.begin(); a != r.end(); ++a)
                                  init += *a;
                              return init;
                          },
                          [](float x, float y) -> float {
                              return x + y;
                          })
                      / initial.size(); // Compute average instead of the sum for better fp precision. The actual pdf is divided later by the size to compensate for it

    if (!std::isfinite(sum)) {
        IG_LOG(L_ERROR) << "Computing hierachical CDF for " << out << " failed due to containing non-finite numbers" << std::endl;
    } else {
        tbb::parallel_for(
            tbb::blocked_range<float*>(initial.data(), initial.data() + initial.size()),
            [sum](const tbb::blocked_range<float*>& r) {
                for (float* a = r.begin(); a != r.end(); ++a)
                    *a /= sum;
            });
    }

    // Ensure the map is a square
    slice = std::min(image.width, image.height);
    if (image.width < image.height)
        initial = downsampleY(initial, image.width, image.height, image.width);
    else if (image.width > image.height)
        initial = downsampleX(initial, image.width, image.height, image.height);

    std::vector<std::vector<float>> data;
    data.reserve((size_t)std::log2(slice));
    data.emplace_back(std::move(initial));

    // Compute mipmaps
    size_t slice2 = slice;
    while (true) {
        auto vec = computeMipMap(data.back(), slice2);
        if (vec.empty())
            break;

        slice2 /= 2;
        data.emplace_back(std::move(vec));
    }

    // Compute necessary information
    levels = data.size();

    size = 0;
    for (const auto& level : data)
        size += level.size();

#if 0
    std::cout << levels << std::endl;
    for (size_t i = 0; i < data.size(); ++i) {
        Image::save("level_" + std::to_string(i) + ".exr", data.at(i).data(), slice / (1 << i), slice / (1 << i), 1);
    }
#endif

    // Write data to file
    FileSerializer serializer(out, false);
    serializer.write(data, true);
}

} // namespace IG