#include "VMFGuidedPathTechnique.h"
#include "Logger.h"
#include "light/Light.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/ShaderUtils.h"

namespace IG {
constexpr int DefaultLearnIterations = 128;
constexpr float DefaultDecayFactor   = 0.85f;
VMFGuidedPathTechnique::VMFGuidedPathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("vmf")
    , mTechnique(obj)
{
    mLightSelector = obj->property("light_selector").getString();
    mEnableNEE     = obj->property("nee").getBool(true);
    mAOVs          = obj->property("aov").getBool(false);
}

static std::string vmf_before_iteration_generator(LoaderContext& ctx, SceneObject& tech)
{
    ShadingTree tree(ctx);
    tree.addInteger("learn_iterations", tech, DefaultLearnIterations, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    tree.addNumber("learn_decay_factor", tech, DefaultDecayFactor, ShadingTree::NumberOptions::Dynamic().MakeGlobal());

    std::stringstream stream;
    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << tree.pullHeader()
           << "  vmf_handle_before_iteration(device"
           << ", settings.iter"
           << ", " << tree.getInline("learn_iterations")
           << ", scene_bbox"
           << ", " << tree.getInline("learn_decay_factor")
           << ");" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

TechniqueInfo VMFGuidedPathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mAOVs)
        info.Passes[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;

    info.UsesLights                                        = true;
    info.CallbackGenerators[(int)CallbackType::BeforePass] = [this](LoaderContext& ctx) { return vmf_before_iteration_generator(ctx, *mTechnique); };
    info.PrimaryPayloadCount                               = 12;
    info.EmitterPayloadInitializer                         = "make_simple_payload_initializer(init_vmf_raypayload)";
    return info;
}

void VMFGuidedPathTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addNumber("defensive", *mTechnique, 0.3f, ShadingTree::NumberOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("learn_iterations", *mTechnique, DefaultLearnIterations, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = make_vmf_renderer(device"
                 << ", " << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", spi"
                 << ", light_selector"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false")
                 << ", " << (mAOVs ? "true" : "false")
                 << ", " << input.Tree.getInline("defensive")
                 << ", settings.iter <= " << input.Tree.getInline("learn_iterations")
                 << ", scene_bbox"
                 << ");" << std::endl;
}

} // namespace IG