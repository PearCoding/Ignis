#pragma once

#include "IG_Config.h"

namespace IG {
struct ImageRgba32 {
	std::unique_ptr<float[]> pixels;
	size_t width, height;

    inline bool isValid() const { return pixels != nullptr; }
    void applyGammaCorrection();
    void flipY();

    static ImageRgba32 load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
    static bool save(const std::filesystem::path& path, const float* rgb, size_t width, size_t height);
};

struct ImageRgb24 {
	std::unique_ptr<float[]> pixels;
	size_t width, height;

    inline bool isValid() const { return pixels != nullptr; }
    void applyGammaCorrection();
    void flipY();

    static ImageRgb24 load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
    static bool save(const std::filesystem::path& path, const float* rgb, size_t width, size_t height);
};
} // namespace IG
