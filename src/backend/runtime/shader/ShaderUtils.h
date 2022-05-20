#pragma once

#include "loader/ShadingTree.h"
#include "Target.h"

namespace IG {
class ShaderUtils {
public:
    static std::string constructDevice(Target target);
    static std::string generateDatabase();
    static std::string generateMaterialShader(ShadingTree& tree, size_t mat_id, bool requireLights, const std::string_view& output_var);

    /// Will generate technique predefinition, function specification and device
    static std::string beginCallback(const LoaderContext& ctx);
    static std::string endCallback();

    static std::string inlineSPI(const LoaderContext& ctx);
};
} // namespace IG