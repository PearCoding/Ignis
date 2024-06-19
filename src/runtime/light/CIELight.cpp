#include "CIELight.h"
#include "Logger.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/ShaderBuilder.h"
#include "skysun/SunLocation.h"

namespace IG {
CIELight::CIELight(CIEType classification, const std::string& name, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mClassification(classification)
    , mLight(light)
{
    mSunDirection = LoaderUtils::getDirection(*light);
    mHasGround    = mLight->property("has_ground").getBool(true);
}

float CIELight::computeFlux(ShadingTree& tree) const
{
    // TODO: Use new bake system!
    const float radius = tree.context().SceneDiameter / 2;
    const float zenith = tree.computeNumber("zenith", *mLight, 1.0f).Value;
    return zenith * Pi * radius * radius * (mHasGround ? 1.0f : 0.5f);
}

// TODO: Map this to Artic
static inline float skylight_normalization_factor(float altitude, bool clear)
{
    constexpr std::array<float, 5> ClearApprox  = { 2.766521f, 0.547665f, -0.369832f, 0.009237f, 0.059229f };
    constexpr std::array<float, 5> IntermApprox = { 3.5556f, -2.7152f, -1.3081f, 1.0660f, 0.60227f };

    const float x   = (altitude - Pi4) / Pi4;
    const auto& arr = clear ? ClearApprox : IntermApprox;
    float f         = arr[4];
    for (int i = 3; i >= 0; --i)
        f = f * x + arr[i];
    return f;
}

void CIELight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("scale", *mLight, Vector3f::Ones());
    input.Tree.addColor("zenith", *mLight, Vector3f::Ones());
    input.Tree.addColor("ground", *mLight, Vector3f::Ones());
    input.Tree.addNumber("ground_brightness", *mLight, 0.2f);

    const Matrix3f trans = mLight->property("transform").getTransform().linear().transpose().inverse();

    if (mClassification == CIEType::Uniform || mClassification == CIEType::Cloudy) {
        bool cloudy = mClassification == CIEType::Cloudy;

        const std::string light_id = input.Tree.currentClosureID();
    
        std::stringstream stream;
        stream << "let light_" << light_id << " = make_cie_sky_light(" << input.ID
               << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
               << ", " << input.Tree.getInline("zenith")
               << ", " << input.Tree.getInline("ground")
               << ", " << input.Tree.getInline("ground_brightness")
               << ", " << (cloudy ? "true" : "false")
               << ", " << (mHasGround ? "true" : "false")
               << ", " << LoaderUtils::inlineMatrix(trans) << ");";
        input.Tree.shader().addStatement(stream.str());
    } else {
        auto ea = LoaderUtils::getEA(*mLight);
        if (ea.Elevation > 87 * Deg2Rad) {
            IG_LOG(L_WARNING) << " Sun too close to zenith, reducing elevation to 87 degrees" << std::endl;
            ea.Elevation = 87 * Deg2Rad;
        }

        const float turbidity = mLight->property("turbidity").getNumber(2.45f);

        // TODO: Map this to Artic
        constexpr float SkyIllum = 203;
        float zenithbrightness   = (1.376f * turbidity - 1.81f) * std::tan(ea.Elevation) + 0.38f;
        if (mClassification == CIEType::Intermediate)
            zenithbrightness = (zenithbrightness + 8.6f * mSunDirection.y() + 0.123f) / 2;
        zenithbrightness = std::max(0.0f, zenithbrightness * 1000 / SkyIllum);

        float factor = 0;
        if (mClassification == CIEType::Clear)
            factor = 0.274f * (0.91f + 10 * std::exp(-3 * (Pi2 - ea.Elevation)) + 0.45f * mSunDirection.y() * mSunDirection.y());
        else
            factor = (2.739f + 0.9891f * std::sin(0.3119f + 2.6f * ea.Elevation)) * std::exp(-(Pi2 - ea.Elevation) * (0.4441f + 1.48f * ea.Elevation));

        const float norm_factor = skylight_normalization_factor(ea.Elevation, mClassification == CIEType::Clear) * InvPi / factor;

        constexpr float SunIllum    = 208;
        const float solarbrightness = 1.5e9f / SunIllum * (1.147f - 0.147f / std::max(mSunDirection.y(), 0.16f));
        const float additive_factor = 6e-5f * InvPi * solarbrightness * mSunDirection.y() * (mClassification == CIEType::Clear ? 1 : 0.15f /* Fudge factor */);

        const float c2 = zenithbrightness * norm_factor + additive_factor;

        const std::string light_id = input.Tree.currentClosureID();

        std::stringstream stream;
        stream << "let light_" << light_id << " = make_cie_sunny_light(" << input.ID
               << ", " << LoaderUtils::inlineSceneBBox(input.Tree.context())
               << ", " << input.Tree.getInline("scale")
               << ", " << input.Tree.getInline("zenith")
               << ", " << zenithbrightness / factor
               << ", " << input.Tree.getInline("ground")
               << ", " << input.Tree.getInline("ground_brightness")
               << ", " << (mClassification == CIEType::Clear ? "true" : "false")
               << ", " << (mHasGround ? "true" : "false")
               << ", " << LoaderUtils::inlineVector(mSunDirection)
               << ", " << c2
               << ", " << LoaderUtils::inlineMatrix(trans) << ");";
        input.Tree.shader().addStatement(stream.str());
    }

    input.Tree.endClosure();
}

} // namespace IG