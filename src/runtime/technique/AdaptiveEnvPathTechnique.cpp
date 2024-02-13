#include "AdaptiveEnvPathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
AdaptiveEnvPathTechnique::AdaptiveEnvPathTechnique(SceneObject& obj)
    : Technique("adaptive_env")
{
    mMaxDepth              = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mMinDepth              = obj.property("min_depth").getInteger(DefaultMinRayDepth);
    mLightSelector         = obj.property("light_selector").getString();
    mEnableNEE             = obj.property("nee").getBool(false);
    mClamp                 = obj.property("clamp").getNumber(0.0f);
    mNumLearningIterations = (size_t)std::max(1, obj.property("learning_iterations").getInteger(1));
}

TechniqueInfo AdaptiveEnvPathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    // We got two passes. (0 -> Learning, 1 -> Sampling)
    info.Variants.resize(2);
    info.Variants[0].UsesLights = true;
    info.Variants[1].UsesLights = true;

    info.Variants[0].PrimaryPayloadCount = 11;
    info.Variants[1].PrimaryPayloadCount = 6;

    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_adaptive_env_learning_raypayload)";
    info.Variants[1].EmitterPayloadInitializer = "make_simple_payload_initializer(init_adaptive_env_sampling_raypayload)";

    // The learning setup requires a CDF construction pass
    info.Variants[0].CallbackGenerators[(int)CallbackType::BeforeIteration] = [](const LoaderContext& ctx) {
        // Reset learning
        std::stringstream stream;

        stream << ShaderUtils::beginCallback(ctx) << std::endl
               << "  aept_handle_before_iteration_learning(device, settings.iter);" << std::endl
               << ShaderUtils::endCallback() << std::endl;

        return stream.str();
    };
    const size_t numLearn                                                  = mNumLearningIterations;
    info.Variants[0].CallbackGenerators[(int)CallbackType::AfterIteration] = [numLearn](const LoaderContext& ctx) {
        // Construct CDF
        std::stringstream stream;

        stream << ShaderUtils::beginCallback(ctx) << std::endl
               << "  aept_handle_after_iteration_learning(device, settings.iter, " << numLearn - 1 << ");" << std::endl
               << ShaderUtils::endCallback() << std::endl;

        return stream.str();
    };

    // info.Variants[0].OverrideSPI     = 1;
    info.Variants[0].LockFramebuffer = true; // We do not change the framebuffer

    info.EnabledAOVs.emplace_back("Guiding");
    info.EnabledAOVs.emplace_back("Guiding PDF");
    info.EnabledAOVs.emplace_back("Guiding Count");

    // TODO: We could increase the learning phase using a user parameter
    info.VariantSelector = [numLearn](size_t iteration) {
        if (iteration < numLearn)
            return std::vector<size_t>{ 0 };
        else
            return std::vector<size_t>{ 1 };
    };

    return info;
}

void AdaptiveEnvPathTechnique::generateBody(const SerializationInput& input) const
{
    const bool is_learning_pass = input.Context.CurrentTechniqueVariant == 0;

    // Insert config into global registry
    input.Context.GlobalRegistry.IntParameters["__tech_max_depth"] = (int)mMaxDepth;
    input.Context.GlobalRegistry.IntParameters["__tech_min_depth"] = (int)mMinDepth;
    input.Context.GlobalRegistry.FloatParameters["__tech_clamp"]   = mClamp;

    // Load registry information
    if (mMaxDepth < 2 && input.Context.Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 & 1 can be an optimization
        input.Stream << "  let tech_max_camera_depth = " << mMaxDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_camera_depth = registry::get_global_parameter_i32(\"__tech_max_depth\", 8);" << std::endl;

    if (mMinDepth < 2 && input.Context.Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 & 1 can be an optimization
        input.Stream << "  let tech_min_camera_depth = " << mMinDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_min_camera_depth = registry::get_global_parameter_i32(\"__tech_min_depth\", 2);" << std::endl;

    if (mClamp <= 0 && input.Context.Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 is a special case
        input.Stream << "  let tech_clamp = " << mClamp << ":f32;" << std::endl;
    else
        input.Stream << "  let tech_clamp = registry::get_global_parameter_f32(\"__tech_clamp\", 0);" << std::endl;

    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree);

    if (is_learning_pass) {
        input.Stream << "  let technique = make_adaptive_env_learning_path_renderer(device, tech_max_camera_depth, tech_min_camera_depth, spi, light_selector, tech_clamp, " << (mEnableNEE ? "true" : "false") << ");" << std::endl;
    } else {
        input.Stream << "  let technique = make_adaptive_env_sampling_path_renderer(device, tech_max_camera_depth, tech_min_camera_depth, spi, light_selector, tech_clamp, " << (mEnableNEE ? "true" : "false") << ");" << std::endl;
    }
}

} // namespace IG