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
std::string TraversalShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_traversal_shader(settings: &Settings, size: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
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

    if (ctx.EntityCount == 0) {
        stream << begin(ctx) << std::endl
               << "  maybe_unused(size);" << std::endl
               << end();
    } else {

        stream << begin(ctx)
               << setupInternal(ctx)
               << "  device.handle_traversal_primary(tracer, size);" << std::endl
               << end();
    }

    return stream.str();
}

std::string TraversalShader::setupSecondary(const LoaderContext& ctx)
{
    const bool is_advanced = ctx.CurrentTechniqueVariantInfo().ShadowHandlingMode != ShadowHandlingMode::Simple;

    std::stringstream stream;

    if (ctx.EntityCount == 0) {
        stream << begin(ctx) << std::endl
               << "  maybe_unused(size);" << std::endl
               << end();
    } else {
        stream << begin(ctx) << std::endl
               << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
               << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
               << setupInternal(ctx)
               << "  device.handle_traversal_secondary(tracer, size, " << (is_advanced ? "true" : "false") << ", spi, use_framebuffer);" << std::endl
               << end();
    }
    return stream.str();
}

std::string TraversalShader::setupInternal(const LoaderContext& ctx)
{
    std::stringstream stream;

    size_t i = 0;
    for (const auto& p : ctx.Shapes->providers()) {
        stream << "  fn @prim_type_" << i++ << "() -> SceneGeometry {" << std::endl;

        // Will define `trace` and `handler`
        stream << p.second->generateTraversalCode(ctx);

        stream << "    let bvh = device.load_scene_bvh(\"" << p.second->identifier() << "\");" << std::endl
               << "    SceneGeometry { database = trace, bvh = bvh, handle_local = handler }" << std::endl
               << "  }" << std::endl;
    }

    stream << std::endl;
    for (size_t k = 0; k < i; ++k)
        stream << "  let prim_type_v_" << k << " = prim_type_" << k << "();" << std::endl;
    stream << std::endl;

    stream << "  fn @tracer_f(i:i32) -> SceneGeometry { match i {" << std::endl;
    for (size_t k = 0; k < i - 1; ++k)
        stream << "    " << k << " => prim_type_v_" << k << "," << std::endl;
    stream << "    _ => prim_type_v_" << (i - 1) << "," << std::endl
           << "  } }" << std::endl
           << "  let tracer = SceneTracer { type_count = " << i << ", get = tracer_f };" << std::endl;

    return stream.str();
}
} // namespace IG