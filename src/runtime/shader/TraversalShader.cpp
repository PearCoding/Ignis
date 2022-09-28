#include "TraversalShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/Loader.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderShape.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string TraversalShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_traversal_shader(settings: &Settings, size: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << "  let entities = load_entity_table(device); maybe_unused(entities);" << std::endl;

    return stream.str();
}

std::string TraversalShader::end()
{
    return "}";
}

std::string TraversalShader::setupPrimary(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << begin(ctx);

    size_t i = 0;
    for (const auto& p : ctx.Shapes->providers()) {
        stream << "  fn @prim_type_" << i++ << "() -> (){" << std::endl;

        // Will define `trace`
        stream << p.second->generateTraversalCode(ctx);

        stream << "  let bvh  = device.load_scene_bvh(\"" << p.second->identifier() << "\");" << std::endl
               << "  let geom = SceneGeometry { database = trace, bvh = bvh };" << std::endl
               << "  device.handle_traversal_primary(geom, size);" << std::endl
               << "  }" << std::endl;
    }

    stream << std::endl;

    for (size_t k = 0; k < i; ++k)
        stream << "  prim_type_" << k << "();" << std::endl;

    stream << end();
    return stream.str();
}

std::string TraversalShader::setupSecondary(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << begin(ctx) << std::endl
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl;

    size_t i = 0;
    for (const auto& p : ctx.Shapes->providers()) {
        stream << "  fn @prim_type_" << i++ << "() -> (){" << std::endl;

        // Will define `trace`
        stream << p.second->generateTraversalCode(ctx);

        bool is_advanced = ctx.CurrentTechniqueVariantInfo().ShadowHandlingMode != ShadowHandlingMode::Simple;
        stream << "  let bvh  = device.load_scene_bvh(\"" << p.second->identifier() << "\");" << std::endl
               << "  let geom = SceneGeometry { database = trace, bvh = bvh };" << std::endl
               << "  device.handle_traversal_secondary(geom, size, " << (is_advanced ? "true" : "false") << ", spi, use_framebuffer);" << std::endl
               << "  }" << std::endl;
    }

    stream << std::endl;

    for (size_t k = 0; k < i; ++k)
        stream << "  prim_type_" << k << "();" << std::endl;

    stream << end();
    return stream.str();
}
} // namespace IG