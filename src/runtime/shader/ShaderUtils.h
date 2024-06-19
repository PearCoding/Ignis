#pragma once

#include "device/Target.h"
#include "loader/ShadingTree.h"

namespace IG {
class ShapeProvider;
class ShaderBuilder;
class ShaderUtils {
public:
    static ShaderBuilder constructDevice(const LoaderContext& ctx);
    static ShaderBuilder generateDatabase(const LoaderContext& ctx);
    static ShaderBuilder generateShapeLookup(const LoaderContext& ctx);
    static ShaderBuilder generateShapeLookup(const std::string& varname, ShapeProvider* provider, const LoaderContext& ctx);
    static ShaderBuilder generateSceneBBox(const LoaderContext& ctx);
    static ShaderBuilder generateScene(const LoaderContext& ctx, bool embed);
    static std::string generateMaterialShader(ShadingTree& tree, size_t mat_id, bool requireLights, const std::string_view& output_var);

    static std::string inlineSPI(const LoaderContext& ctx);
    static std::string inlinePayloadInfo(const LoaderContext& ctx);
};
} // namespace IG