#include "IO.h"
#include "Image.h"
#include "ImageIO.h"
#include "Runtime.h"

IG_BEGIN_IGNORE_WARNINGS
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
IG_END_IGNORE_WARNINGS

namespace IG {
bool saveImageRGB(const std::filesystem::path& path, const float* rgb, size_t width, size_t height, float scale)
{
    ImageRgba32 img;
    img.width  = width;
    img.height = height;
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

bool saveImageRGBA(const std::filesystem::path& path, const float* rgb, size_t width, size_t height, float scale)
{
    ImageRgba32 img;
    img.width  = width;
    img.height = height;
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

bool saveImageOutput(const std::filesystem::path& path, const Runtime& runtime)
{
    const size_t width  = runtime.framebufferWidth();
    const size_t height = runtime.framebufferHeight();
    float scale         = 1.0f / runtime.currentIterationCount();
    if (runtime.currentIterationCount() == 0)
        scale = 0;

    // size_t aov_count = runtime.aovs().size() + 1;
    size_t aov_count = 1;

    // std::cout<< "in Io.cpp" << aov_count << std::endl;
    std::vector<float> images(width * height * 3 * aov_count);

    // Copy data
    for (size_t aov = 0; aov < aov_count; ++aov) {
        const float* src = runtime.getFramebuffer((int)aov);
        float* dst_r     = &images[width * height * (3 * aov + 0)];
        float* dst_g     = &images[width * height * (3 * aov + 1)];
        float* dst_b     = &images[width * height * (3 * aov + 2)];

        const auto pixelF = [&](size_t ind) {
            float r = src[ind * 3 + 0];
            float g = src[ind * 3 + 1];
            float b = src[ind * 3 + 2];

            dst_r[ind] = r * scale;
            dst_g[ind] = g * scale;
            dst_b[ind] = b * scale;
        };

        tbb::parallel_for(tbb::blocked_range<size_t>(0, width * height),
                          [&](tbb::blocked_range<size_t> r) {
                              for (size_t i = r.begin(); i < r.end(); ++i)
                                  pixelF(i);
                          });
    }

    std::vector<const float*> image_ptrs(3 * aov_count);
    std::vector<std::string> image_names(3 * aov_count);
    for (size_t aov = 0; aov < aov_count; ++aov) {
        // Swizzle RGB to BGR as some viewers expect it per default
        image_ptrs[3 * aov + 2] = &images[width * height * (3 * aov + 0)];
        image_ptrs[3 * aov + 1] = &images[width * height * (3 * aov + 1)];
        image_ptrs[3 * aov + 0] = &images[width * height * (3 * aov + 2)];

        // Framebuffer
        if (aov == 0) {
            // std::cout<< "in If" << aov_count << std::endl;
            if (aov_count == 1) {// wont go into this if since aov_count = 3
                // If we have no aovs, stick to the standard naming
                image_names[3 * aov + 0] = "B";
                image_names[3 * aov + 1] = "G";
                image_names[3 * aov + 2] = "R";
            }
            else {
                // std::cout<< "in If-else" << aov_count << std::endl;
                image_names[3 * aov + 0] = "Default.B";
                image_names[3 * aov + 1] = "Default.G";
                image_names[3 * aov + 2] = "Default.R";
            }
        } else {
            std::string name         = runtime.aovs()[aov - 1];
            std::cout<< "aov_names: " << name << std::endl;
            image_names[3 * aov + 0] = name + ".B";
            image_names[3 * aov + 1] = name + ".G";
            image_names[3 * aov + 2] = name + ".R";
        }
    }

    std::vector<const float*> img_ptrs;
    std::vector<std::string> img_names;

    std::cout<< "ptr.size: " << image_ptrs.size() << std::endl;
    std::cout<< "names.size: " << image_names.size() << std::endl;
    for (int aovs = 0; aovs < (int)image_names.size(); aovs++){
        img_ptrs.push_back(image_ptrs[aovs]);
        img_names.push_back(image_names[aovs]);
        if((aovs + 1) % 3 == 0){
            std::cout<< "img_names.size: " << img_names.size() << "cond" << (aovs + 1) << std::endl;
            auto aov_ret = ImageIO::save(path, width, height, img_ptrs, img_names);
            if(!aov_ret){
                std::cout << "Failed to save " << img_names[0] << std::endl;
                return false;
            }
            img_ptrs.clear();
            img_names.clear();
            break;
        }
    }

        // auto aov_ret = ImageIO::save(path, width, height, img_ptrs, img_names);
    // ImageIO::save(path, width, height, image_ptrs, image_names);

    // return ImageIO::save(path, width, height, image_ptrs, image_names);
    return true;
}
} // namespace IG