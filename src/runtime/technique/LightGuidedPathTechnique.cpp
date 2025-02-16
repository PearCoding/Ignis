#include "LightGuidedPathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/ShaderUtils.h"

namespace IG {
constexpr int DefaultLearnIterations = 128;
constexpr float DefaultDecayFactor   = 0.85f;
LightGuidedPathTechnique::LightGuidedPathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("lsgpt")
    , mTechnique(obj)
{
    mLightSelector  = obj->property("light_selector").getString();
    mEnableNEE      = obj->property("nee").getBool(true);
    mAOVs           = obj->property("aov").getBool(false);
    mLearnDefensive = obj->property("learn_defensive").getBool(true);
}

static std::string vgpt_before_iteration_generator(LoaderContext& ctx, SceneObject& tech)
{
    ShadingTree tree(ctx);
    tree.addInteger("learn_iterations", tech, DefaultLearnIterations, ShadingTree::IntegerOptions::Zero().MakeGlobal());
    tree.addNumber("learn_decay_factor", tech, DefaultDecayFactor, ShadingTree::NumberOptions::Dynamic().MakeGlobal());

    std::stringstream stream;
    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << tree.pullHeader()
           << "  vgpt_handle_before_iteration(device"
           << ", settings.iter"
           << ", " << tree.getInline("learn_iterations")
           << ", scene_bbox"
           << ", " << tree.getInline("learn_decay_factor")
           << ");" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

TechniqueInfo LightGuidedPathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mAOVs)
        info.Passes[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;

    info.UsesLights = true;
    if (mLearnDefensive) {
        info.CallbackGenerators[(int)CallbackType::BeforePass] = [=](LoaderContext& ctx) { return vgpt_before_iteration_generator(ctx, *mTechnique); };
        info.PrimaryPayloadCount                               = 14;
        info.EmitterPayloadInitializer                         = "make_simple_payload_initializer(init_vgpt_raypayload)";
    } else {
        info.PrimaryPayloadCount       = 8;
        info.EmitterPayloadInitializer = "make_simple_payload_initializer(init_sgpt_raypayload)";
    }
    return info;
}

void LightGuidedPathTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addNumber("defensive", *mTechnique, 0.3f, ShadingTree::NumberOptions::Dynamic().MakeGlobal());
    if (mLearnDefensive)
        input.Tree.addInteger("learn_iterations", *mTechnique, DefaultLearnIterations, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());

    const char* renderer = mLearnDefensive ? "make_light_vgpt_renderer" : "make_light_sgpt_renderer";
    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = " << renderer << "(device"
                 << ", " << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", spi"
                 << ", light_selector"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false")
                 << ", " << (mAOVs ? "true" : "false")
                 << ", infinite_lights.get(0) /*TODO*/"
                 << ", " << input.Tree.getInline("defensive");

    if (mLearnDefensive) {
        input.Stream << ", settings.iter <= " << input.Tree.getInline("learn_iterations")
                     << ", scene_bbox";
    }

    input.Stream << ");" << std::endl;
}

} // namespace IG