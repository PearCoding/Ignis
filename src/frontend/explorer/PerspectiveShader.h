#pragma once

#include "loader/LoaderOptions.h"

namespace IG {
struct PerspectiveShader {
    [[nodiscard]] static std::string generate(const LoaderOptions& ctx);
};
} // namespace IG
