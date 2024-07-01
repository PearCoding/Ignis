#pragma once

#include "loader/LoaderOptions.h"

namespace IG {
struct ShaderGenerator {
    [[nodiscard]] static std::string generatePerspective(const LoaderOptions& ctx);
    [[nodiscard]] static std::string generateTonemap(const LoaderOptions& ctx);
    [[nodiscard]] static std::string generateGlare(const LoaderOptions& ctx);
};
} // namespace IG
