#include "InfoBufferTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/Parser.h"

namespace IG {
InfoBufferTechnique::InfoBufferTechnique(SceneObject& obj)
    : Technique("infobuffer")
{
    mMaxDepth       = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mFollowSpecular = obj.property("handle_specular").getBool(false);
    mAllIterations  = obj.property("all_iterations").getBool(false); // True if every iterations, else only first one
}

TechniqueInfo InfoBufferTechnique::getInfo(const LoaderContext& ctx) const
{
    const bool apply_always = !ctx.Options.Denoiser.OnlyFirstIteration || mAllIterations;

    TechniqueInfo info;
    enable(info, apply_always, false);
    return info;
}

void InfoBufferTechnique::generateBody(const SerializationInput& input) const
{
    insertBody(input, mMaxDepth, mFollowSpecular);
}

void InfoBufferTechnique::enable(TechniqueInfo& info, bool always, bool extend)
{
    info.EnabledAOVs.emplace_back("Normals");
    info.EnabledAOVs.emplace_back("Albedo");
    info.EnabledAOVs.emplace_back("Depth");

    if (extend)
        info.Variants.emplace_back();

    info.Variants.back().LockFramebuffer           = true;
    info.Variants.back().OverrideSPI               = 1;
    info.Variants.back().PrimaryPayloadCount       = 2;
    info.Variants.back().EmitterPayloadInitializer = "make_simple_payload_initializer(init_ib_raypayload)";

    const size_t variantCount = info.Variants.size();
    if (variantCount > 1) {
        if (info.VariantSelector) {
            const auto prevSelector = info.VariantSelector;
            info.VariantSelector    = [=](size_t iter) {
                if (always || iter == 0) {
                    std::vector<size_t> pass_with_ib = prevSelector(iter);
                    pass_with_ib.emplace_back(variantCount - 1);
                    return pass_with_ib;
                } else {
                    return prevSelector(iter);
                }
            };
        } else {
            info.VariantSelector = [=](size_t iter) {
                if (always || iter == 0) {
                    std::vector<size_t> pass_with_ib(variantCount);
                    std::iota(std::begin(pass_with_ib), std::end(pass_with_ib), 0);
                    return pass_with_ib;
                } else {
                    std::vector<size_t> pass_without_ib(variantCount - 1);
                    std::iota(std::begin(pass_without_ib), std::end(pass_without_ib), 0);
                    return pass_without_ib;
                }
            };
        }
    }
}

bool InfoBufferTechnique::insertBody(const SerializationInput& input, size_t maxDepth, bool followSpecular)
{
    const auto& info = input.Context.Technique->info();
    if (!input.Context.Technique->hasDenoiserEnabled())
        return false;

    if (input.Context.CurrentTechniqueVariant != info.Variants.size() - 1)
        return false;

    const bool handle_specular = input.Context.Options.Denoiser.FollowSpecular || followSpecular;

    // Insert config into global registry
    input.Context.GlobalRegistry.IntParameters["__tech_ib_max_depth"] = (int)maxDepth;

    if (maxDepth < 2) // 0 & 1 can be an optimization
        input.Stream << "  let tech_max_depth = " << maxDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_depth = registry::get_global_parameter_i32(\"__tech_ib_max_depth\", 8);" << std::endl;

    input.Stream << "  let aov_normals = device.load_aov_image(\"Normals\", spi); aov_normals.mark_as_used();" << std::endl
                 << "  let aov_albedo = device.load_aov_image(\"Albedo\", spi); aov_albedo.mark_as_used();" << std::endl
                 << "  let aov_depth  = device.load_aov_image(\"Depth\", spi); aov_depth.mark_as_used();" << std::endl
                 << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                 << "    match(id) {" << std::endl
                 << "      0x1000 => aov_normals," << std::endl
                 << "      0x1001 => aov_albedo," << std::endl
                 << "      0x1002 => aov_depth," << std::endl
                 << "      _ => make_empty_aov_image()" << std::endl
                 << "    }" << std::endl
                 << "  };" << std::endl
                 << "  let technique = make_infobuffer_renderer(tech_max_depth, aovs, " << (handle_specular ? "true" : "false") << ");" << std::endl;

    return true;
}

} // namespace IG