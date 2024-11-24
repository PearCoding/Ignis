#include "AdaptiveEnvPathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

// Currently experimental and not improving scene!
namespace IG {
AdaptiveEnvPathTechnique::AdaptiveEnvPathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("adaptive_env")
    , mTechnique(obj)
{
    mLightSelector         = obj->property("light_selector").getString();
    mEnableNEE             = obj->property("nee").getBool(false);
    mNumLearningIterations = (size_t)std::max(1, obj->property("learning_iterations").getInteger(1));
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
    const bool is_learning_pass = input.Tree.context().CurrentTechniqueVariant == 0;

    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree);

    if (is_learning_pass) {
        input.Stream << "  let technique = make_adaptive_env_learning_path_renderer(device"
                     << ", " << input.Tree.getInline("max_depth")
                     << ", " << input.Tree.getInline("min_depth")
                     << ", spi, light_selector"
                     << ", " << input.Tree.getInline("clamp")
                     << ", " << (mEnableNEE ? "true" : "false") << ");" << std::endl;
    } else {
        input.Stream << "  let technique = make_adaptive_env_sampling_path_renderer(device"
                     << ", " << input.Tree.getInline("max_depth")
                     << ", " << input.Tree.getInline("min_depth")
                     << ", spi, light_selector"
                     << ", " << input.Tree.getInline("clamp")
                     << ", " << (mEnableNEE ? "true" : "false") << ");" << std::endl;
    }
}

} // namespace IG