#include "LoaderBSDF.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "StringUtils.h"

#include "bsdf/BlendBSDF.h"
#include "bsdf/ConductorBSDF.h"
#include "bsdf/DJMeasuredBSDF.h"
#include "bsdf/DielectricBSDF.h"
#include "bsdf/DiffuseBSDF.h"
#include "bsdf/ErrorBSDF.h"
#include "bsdf/IgnoreBSDF.h"
#include "bsdf/KlemsBSDF.h"
#include "bsdf/MapBSDF.h"
#include "bsdf/MaskBSDF.h"
#include "bsdf/PassthroughBSDF.h"
#include "bsdf/PhongBSDF.h"
#include "bsdf/PlasticBSDF.h"
#include "bsdf/PrincipledBSDF.h"
#include "bsdf/RadBRTDFuncBSDF.h"
#include "bsdf/TensorTreeBSDF.h"
#include "bsdf/TransformBSDF.h"
#include "bsdf/TransparentBSDF.h"

namespace IG {

static std::shared_ptr<BSDF> bsdf_diffuse(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<DiffuseBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_dielectric(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<DielectricBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_conductor(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<ConductorBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_phong(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PhongBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_principled(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PrincipledBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_plastic(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PlasticBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_klems(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<KlemsBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_tensortree(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<TensorTreeBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_djmeasured(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<DJMeasuredBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_add(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<BlendBSDF>(BlendBSDF::Type::Add, name, obj);
}

static std::shared_ptr<BSDF> bsdf_mix(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<BlendBSDF>(BlendBSDF::Type::Mix, name, obj);
}

static std::shared_ptr<BSDF> bsdf_mask(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MaskBSDF>(MaskBSDF::Type::Mask, name, obj);
}

static std::shared_ptr<BSDF> bsdf_cutoff(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MaskBSDF>(MaskBSDF::Type::Cutoff, name, obj);
}

static std::shared_ptr<BSDF> bsdf_passthrough(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PassthroughBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_bumpmap(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MapBSDF>(MapBSDF::Type::Bump, name, obj);
}

static std::shared_ptr<BSDF> bsdf_normalmap(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MapBSDF>(MapBSDF::Type::Normal, name, obj);
}

static std::shared_ptr<BSDF> bsdf_transform(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<TransformBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_ignore(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<IgnoreBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_doublesided(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PlasticBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_rad_brtdfunc(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<RadBRTDFuncBSDF>(name, obj);
}

static std::shared_ptr<BSDF> bsdf_transparent(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<TransparentBSDF>(name, obj);
}

using BSDFLoader = std::shared_ptr<BSDF> (*)(const std::string& name, const std::shared_ptr<SceneObject>& obj);
static const struct {
    const char* Name;
    BSDFLoader Loader;
} _generators[] = {
    { "diffuse", bsdf_diffuse },
    { "roughdiffuse", bsdf_diffuse },       // Deprecated
    { "glass", bsdf_dielectric },           // Deprecated
    { "dielectric", bsdf_dielectric },
    { "roughdielectric", bsdf_dielectric }, // Deprecated
    { "thindielectric", bsdf_dielectric },  // Deprecated
    { "mirror", bsdf_conductor },           // Deprecated
    { "conductor", bsdf_conductor },
    { "roughconductor", bsdf_conductor },   // Deprecated
    { "phong", bsdf_phong },
    { "principled", bsdf_principled },
    { "plastic", bsdf_plastic },
    { "roughplastic", bsdf_plastic }, // Deprecated
    { "klems", bsdf_klems },
    { "tensortree", bsdf_tensortree },
    { "djmeasured", bsdf_djmeasured },
    { "add", bsdf_add },
    { "blend", bsdf_mix },
    { "mix", bsdf_mix },
    { "mask", bsdf_mask },
    { "cutoff", bsdf_cutoff },
    { "passthrough", bsdf_passthrough },
    { "null", bsdf_passthrough },
    { "bumpmap", bsdf_bumpmap },
    { "normalmap", bsdf_normalmap },
    { "transform", bsdf_transform },
    { "twosided", bsdf_ignore }, // Deprecated
    { "doublesided", bsdf_doublesided },
    { "rad_brtdfunc", bsdf_rad_brtdfunc },
    { "transparent", bsdf_transparent },
    { "", nullptr }
};

void LoaderBSDF::prepare(const LoaderContext& ctx)
{
    for (const auto& pair : ctx.Options.Scene->bsdfs()) {
        const std::string name = pair.first;
        const auto bsdf        = pair.second;
        bool found             = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == bsdf->pluginType()) {
                mAvailableBSDFs.emplace(name, _generators[i].Loader(name, bsdf));
                found = true;
                break;
            }
        }

        if (!found) {
            IG_LOG(L_ERROR) << "No bsdf type '" << bsdf->pluginType() << "' for '" << name << "' available" << std::endl;
            mAvailableBSDFs.emplace(name, std::make_shared<ErrorBSDF>(name));
        }
    }
}

std::string LoaderBSDF::generate(const std::string& name, ShadingTree& tree)
{
    std::stringstream stream;
    auto it = mAvailableBSDFs.find(name);
    if (it == mAvailableBSDFs.end()) {
        IG_LOG(L_ERROR) << "Unknown bsdf '" << name << "'" << std::endl;
        stream << ErrorBSDF::inlineError(tree.getClosureID(name)) << std::endl;
        return stream.str();
    }

    it->second->serialize(BSDF::SerializationInput{ stream, tree });

    return stream.str();
}
} // namespace IG
