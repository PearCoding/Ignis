#include "LoaderLight.h"
#include "CDF.h"
#include "Loader.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "serialization/VectorSerializer.h"

#include "light/AreaLight.h"
#include "light/CIELight.h"
#include "light/DirectionalLight.h"
#include "light/EnvironmentLight.h"
#include "light/PerezLight.h"
#include "light/PointLight.h"
#include "light/SkyLight.h"
#include "light/SpotLight.h"
#include "light/SunLight.h"

#include <algorithm>
#include <chrono>

namespace IG {
static std::shared_ptr<Light> light_point(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<PointLight>(name, light);
}

static std::shared_ptr<Light> light_area(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext& ctx)
{
    return std::make_shared<AreaLight>(name, ctx, light);
}

static std::shared_ptr<Light> light_directional(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<DirectionalLight>(name, light);
}

static std::shared_ptr<Light> light_spot(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<SpotLight>(name, light);
}

static std::shared_ptr<Light> light_sun(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<SunLight>(name, light);
}

static std::shared_ptr<Light> light_sky(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<SkyLight>(name, light);
}

static std::shared_ptr<Light> light_cie_env(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    if (light->pluginType() == "cie_cloudy" || light->pluginType() == "ciecloudy")
        return std::make_shared<CIELight>(CIEType::Cloudy, name, light);
    else if (light->pluginType() == "cie_clear" || light->pluginType() == "cieclear")
        return std::make_shared<CIELight>(CIEType::Clear, name, light);
    else if (light->pluginType() == "cie_intermediate" || light->pluginType() == "cieintermediate")
        return std::make_shared<CIELight>(CIEType::Intermediate, name, light);
    else
        return std::make_shared<CIELight>(CIEType::Uniform, name, light);
}

static std::shared_ptr<Light> light_perez(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<PerezLight>(name, light);
}

static std::shared_ptr<Light> light_env(const std::string& name, const std::shared_ptr<Parser::Object>& light, LoaderContext&)
{
    return std::make_shared<EnvironmentLight>(name, light);
}

using LightLoader = std::shared_ptr<Light> (*)(const std::string&, const std::shared_ptr<Parser::Object>&, LoaderContext&);
static const struct {
    const char* Name;
    LightLoader Loader;
} _generators[] = {
    { "point", light_point },
    { "area", light_area },
    { "directional", light_directional },
    { "direction", light_directional },
    { "distant", light_directional },
    { "spot", light_spot },
    { "sun", light_sun },
    { "sky", light_sky },
    { "cie_uniform", light_cie_env },
    { "cieuniform", light_cie_env },
    { "cie_cloudy", light_cie_env },
    { "ciecloudy", light_cie_env },
    { "cie_clear", light_cie_env },
    { "cieclear", light_cie_env },
    { "cie_intermediate", light_cie_env },
    { "cieintermediate", light_cie_env },
    { "perez", light_perez },
    { "uniform", light_env },
    { "env", light_env },
    { "envmap", light_env },
    { "constant", light_env },
    { "", nullptr }
};

std::string LoaderLight::generate(ShadingTree& tree, bool skipFinite)
{
    // Embed lights if necessary. This is cached for multiple calls
    embedLights(tree);

    std::stringstream stream;

    // Write all non-embedded lights to shader
    size_t counter = embeddedLightCount();
    for (auto light : mInfiniteLights)
        light->serialize(Light::SerializationInput{ counter++, stream, tree });
    if (!skipFinite) {
        for (auto light : mFiniteLights) {
            if (!isEmbedding() || !light->getEmbedClass().has_value())
                light->serialize(Light::SerializationInput{ counter++, stream, tree });
        }
    }

    // Add a new line for cosmetics if necessary :P
    if (counter > 0)
        stream << std::endl;

    // Load embedded lights if necessary
    if (isEmbedding() && !skipFinite) {
        size_t offset = 0;
        for (const auto& p : mEmbedClassCounter) {
            std::string var_name = to_lowercase(p.first);
            stream << "  let e_" << var_name << " = ";

            if (p.first == "SimpleAreaLight")
                stream << "load_simple_area_lights(" << offset << ", device, shapes);" << std::endl;
            else if (p.first == "SimplePlaneLight")
                stream << "load_simple_plane_lights(" << offset << ", device);" << std::endl;
            else if (p.first == "SimplePointLight")
                stream << "load_simple_point_lights(" << offset << ", device);" << std::endl;
            else if (p.first == "SimpleSpotLight")
                stream << "load_simple_spot_lights(" << offset << ", device);" << std::endl;
            else
                IG_LOG(L_ERROR) << "Unknown embed class '" << p.first << "'" << std::endl;

            // Special case: Nothing except embedded simple point lights
            if (p.second == counter) {
                stream << "  let num_lights = " << lightCount() << ";" << std::endl
                       << "  let lights = e_" << var_name << ";" << std::endl;
                return stream.str();
            }

            offset += p.second;
        }
    }

    // Write out basic information and start light table
    stream << "  let num_lights = " << lightCount() << ";" << std::endl
           << "  let lights = @|id:i32| {" << std::endl;

    bool embedded = false;
    if (isEmbedding() && !skipFinite) {
        size_t offset = 0;
        for (const auto& p : mEmbedClassCounter) {
            std::string var_name = to_lowercase(p.first);

            if (offset > 0)
                stream << "    else if ";
            else
                stream << "    if ";

            stream << "id < " << (offset + p.second) << " {" << std::endl
                   << "      e_" << var_name << "(id - " << offset << ")" << std::endl
                   << "    }" << std::endl;

            offset += p.second;
            embedded = true;
        }
    }

    if (embedded)
        stream << "    else {" << std::endl;
    stream << "    match(id) {" << std::endl;

    counter = embeddedLightCount();
    for (auto light : mInfiniteLights)
        stream << "      " << (counter++) << " => light_" << tree.getClosureID(light->name()) << "," << std::endl;
    if (!skipFinite) {
        for (auto light : mFiniteLights) {
            if (!isEmbedding() || !light->getEmbedClass().has_value())
                stream << "      " << (counter++) << " => light_" << tree.getClosureID(light->name()) << "," << std::endl;
        }
    }

    stream << "      _ => make_null_light(id)" << std::endl;

    if (embedded)
        stream << "    }" << std::endl;

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}

void LoaderLight::prepare(const LoaderContext& ctx)
{
    IG_LOG(L_DEBUG) << "Prepare lights" << std::endl;
    findEmissiveEntities(ctx);
}

void LoaderLight::findEmissiveEntities(const LoaderContext& ctx)
{
    // Before we create the actual light objects, we need the entity loader to know the actual emissive entity names
    // to prevent having all the entities in the memory just in case they are flagged 'emissive' later-on.
    const auto& lights = ctx.Scene.lights();
    for (auto pair : lights) {
        const auto light = pair.second;

        if (light->pluginType() == "area") {
            std::string entity = light->property("entity").getString();
            if (!entity.empty())
                mEmissiveEntities.insert(entity);
        }
    }
}

void LoaderLight::setup(LoaderContext& ctx)
{
    IG_LOG(L_DEBUG) << "Setup lights" << std::endl;
    loadLights(ctx);
    setupEmbedClasses();
    sortLights();
    setupAreaLights();
}

void LoaderLight::loadLights(LoaderContext& ctx)
{
    const auto& lights = ctx.Scene.lights();
    for (auto pair : lights) {
        const auto light = pair.second;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == light->pluginType()) {
                auto obj = _generators[i].Loader(pair.first, light, ctx);

                if (obj) {
                    if (obj->isInfinite())
                        mInfiniteLights.push_back(obj);
                    else
                        mFiniteLights.push_back(obj);
                    found = true;
                }
                break;
            }
        }

        if (!found)
            IG_LOG(L_ERROR) << "No light type '" << light->pluginType() << "' available" << std::endl;
    }
}

void LoaderLight::setupEmbedClasses()
{
    // TODO: Currently only finite lights can be embedded
    for (const auto& light : mFiniteLights) {
        auto embedClass = light->getEmbedClass();
        if (embedClass.has_value()) {
            auto it = mEmbedClassCounter.find(embedClass.value());
            if (it == mEmbedClassCounter.end())
                mEmbedClassCounter[embedClass.value()] = 1;
            else
                it->second += 1;
        }
    }
}

void LoaderLight::sortLights()
{
    // Partition based on embed classes
    auto begin = mFiniteLights.begin();
    for (auto p : mEmbedClassCounter) {
        auto it = std::partition(begin, mFiniteLights.end(),
                                 [&](const auto& light) { 
        auto embedClass = light->getEmbedClass();
        return embedClass.value_or("") == p.first; });

        begin = it;
        if (begin == mFiniteLights.end())
            break;
    }

    mTotalEmbedCount = std::distance(mFiniteLights.begin(), begin);
}

void LoaderLight::setupAreaLights()
{
    for (size_t id = 0; id < mFiniteLights.size(); ++id) {
        const auto& light = mFiniteLights[id];
        const auto entity = light->entity();
        if (entity.has_value())
            mAreaLights[entity.value()] = id;
    }
}

void LoaderLight::embedLights(ShadingTree& tree)
{
    if (!isEmbedding())
        return;

    for (const auto& p : mEmbedClassCounter) {
        const auto embedClass = p.first;

        // Check if already loaded
        if (tree.context().Database->CustomTables.count(embedClass) > 0)
            continue;

        auto& tbl = tree.context().Database->CustomTables[embedClass];

        IG_LOG(L_DEBUG) << "Embedding lights of class '" << embedClass << "'" << std::endl;

        for (const auto& light : mFiniteLights) {
            if (light->getEmbedClass().value_or("") != embedClass)
                continue;

            auto& lightData = tbl.addLookup(0, 0, 0); // We do not make use of the typeid
            VectorSerializer lightSerializer(lightData, false);
            light->embed(Light::EmbedInput{ lightSerializer, tree });
        }
    }
}

std::filesystem::path LoaderLight::generateLightSelectionCDF(ShadingTree& tree)
{
    const std::string exported_id = "_light_cdf_";

    const auto data = tree.context().ExportedData.find(exported_id);
    if (data != tree.context().ExportedData.end())
        return std::any_cast<std::string>(data->second);

    if (lightCount() == 0)
        return {}; // Fallback to null light selector

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/light_cdf.bin";

    std::vector<float> estimated_powers;
    estimated_powers.reserve(lightCount());
    for (const auto& light : mInfiniteLights)
        estimated_powers.push_back(light->computeFlux(tree));
    for (const auto& light : mFiniteLights)
        estimated_powers.push_back(light->computeFlux(tree));

    CDF::computeForArray(estimated_powers, path);

    tree.context().ExportedData[exported_id] = path;
    return path;
}
} // namespace IG