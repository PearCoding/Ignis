#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct DeviceShader {
    static std::string setup(const LoaderContext& ctx);
};
} // namespace IG