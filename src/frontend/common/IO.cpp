#include "IO.h"
#include "Image.h"
#include "Runtime.h"

IG_BEGIN_IGNORE_WARNINGS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
IG_END_IGNORE_WARNINGS

namespace IG {
bool saveImageRGB(const Path& path, const float* rgb, size_t width, size_t height, float scale)
{
    Image img;
    img.width    = width;
    img.height   = height;
    img.channels = 4;
    img.pixels.reset(new float[width * height * 4]);

    const auto pixelF = [&](size_t ind) {
        auto r = rgb[ind * 3 + 0];
        auto g = rgb[ind * 3 + 1];
        auto b = rgb[ind * 3 + 2];

        img.pixels[4 * ind + 0] = r * scale;
        img.pixels[4 * ind + 1] = g * scale;
        img.pixels[4 * ind + 2] = b * scale;
        img.pixels[4 * ind + 3] = 1.0f;
    };

    tbb::parallel_for(tbb::blocked_range<size_t>(0, width * height),
                      [&](tbb::blocked_range<size_t> r) {
                          for (size_t i = r.begin(); i < r.end(); ++i)
                              pixelF(i);
                      });

    return img.save(path);
}

bool saveImageRGBA(const Path& path, const float* rgb, size_t width, size_t height, float scale)
{
    Image img;
    img.width    = width;
    img.height   = height;
    img.channels = 4;
    img.pixels.reset(new float[width * height * 4]);

    const auto pixelF = [&](size_t ind) {
        auto r = rgb[ind * 4 + 0];
        auto g = rgb[ind * 4 + 1];
        auto b = rgb[ind * 4 + 2];
        auto a = rgb[ind * 4 + 3];

        img.pixels[4 * ind + 0] = r * scale;
        img.pixels[4 * ind + 1] = g * scale;
        img.pixels[4 * ind + 2] = b * scale;
        img.pixels[4 * ind + 3] = a * scale;
    };

    tbb::parallel_for(tbb::blocked_range<size_t>(0, width * height),
                      [&](tbb::blocked_range<size_t> r) {
                          for (size_t i = r.begin(); i < r.end(); ++i)
                              pixelF(i);
                      });

    return img.save(path);
}
} // namespace IG