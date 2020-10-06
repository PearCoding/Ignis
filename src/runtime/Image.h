#pragma once

#include "IG_Config.h"

namespace IG {
struct ImageRgba32 {
	std::unique_ptr<float[]> pixels;
	size_t width, height;

    inline bool isValid() const { return pixels != nullptr; }
    void applyGammaCorrection();
    
    static ImageRgba32 load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
};
} // namespace IG
