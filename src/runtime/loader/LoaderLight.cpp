#include "LoaderLight.h"
#include "CDF.h"
#include "Loader.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "StringUtils.h"
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

#include "light/LightHierarchy.h"

#include <algorithm>
#include <chrono>

namespace IG {
static std::shared_ptr<Light> light_point(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<PointLight>(name, light);
}

static std::shared_ptr<Light> light_area(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext& ctx)
{
    return std::make_shared<AreaLight>(name, ctx, light);
}

static std::shared_ptr<Light> light_directional(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<DirectionalLight>(name, light);
}

static std::shared_ptr<Light> light_spot(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<SpotLight>(name, light);
}

static std::shared_ptr<Light> light_sun(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<SunLight>(name, light);
}

static std::shared_ptr<Light> light_sky(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<SkyLight>(name, light);
}

static std::shared_ptr<Light> light_cie_env(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
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

static std::shared_ptr<Light> light_perez(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<PerezLight>(name, light);
}

static std::shared_ptr<Light> light_env(const std::string& name, const std::shared_ptr<SceneObject>& light, LoaderContext&)
{
    return std::make_shared<EnvironmentLight>(name, light);
}

using LightLoader = std::shared_ptr<Light> (*)(const std::string&, const std::shared_ptr<SceneObject>&, LoaderContext&);
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
    std::stringstream stream;

    stream << generateInfinite(tree);

    if (skipFinite)
        stream << "  let finite_lights = make_proxy_light_table(" << finiteLightCount() << ");" << std::endl
               << "  maybe_unused(finite_lights);" << std::endl;
    else
        stream << generateFinite(tree);

    return stream.str();
}

std::string LoaderLight::generateInfinite(ShadingTree& tree)
{
    std::stringstream stream;

    // Write all non-embedded lights to shader
    size_t counter = 0;
    for (auto light : mInfiniteLights) {
        IG_ASSERT(light->id() == counter, "Expected id to match in generate");
        light->serialize(Light::SerializationInput{ counter++, stream, tree });
    }

    // Write out basic information and start light table
    stream << "  let infinite_lights = LightTable {" << std::endl
           << "    count = " << infiniteLightCount() << "," << std::endl
           << "    get   = @|id:i32| {" << std::endl
           << "      match(id) {" << std::endl;

    counter = 0;
    for (auto light : mInfiniteLights)
        stream << "      " << (counter++) << " => light_" << tree.getClosureID(light->name()) << "," << std::endl;

    stream << "      _ => make_null_light(id)" << std::endl
           << "    }" << std::endl
           << "  }};" << std::endl
           << "  maybe_unused(infinite_lights);" << std::endl;

    return stream.str();
}

std::string LoaderLight::generateFinite(ShadingTree& tree)
{
    // Embed lights if necessary. This is cached for multiple calls
    embedLights(tree);

    std::stringstream stream;

    // Write all non-embedded lights to shader
    size_t counter = embeddedLightCount();
    for (auto light : mFiniteLights) {
        if (!isEmbedding() || !light->getEmbedClass().has_value()) {
            IG_ASSERT(light->id() == counter, "Expected id to match in generate");
            light->serialize(Light::SerializationInput{ counter++, stream, tree });
        }
    }

    // Add a new line for cosmetics if necessary :P
    if (counter > 0)
        stream << std::endl;

    // Load embedded lights if necessary
    if (isEmbedding()) {
        size_t offset = 0;
        for (const auto& p : mEmbedClassCounter) {
            std::string var_name = to_lowercase(p.first);
            stream << "  let e_" << var_name << " = ";

            if (p.first == "SimpleAreaLight")
                stream << "load_simple_area_lights(" << p.second << ", " << offset << ", device, shapes);" << std::endl;
            else if (p.first == "SimplePlaneLight")
                stream << "load_simple_plane_lights(" << p.second << ", " << offset << ", device);" << std::endl;
            else if (p.first == "SimplePointLight")
                stream << "load_simple_point_lights(" << p.second << ", " << offset << ", device);" << std::endl;
            else if (p.first == "SimpleSpotLight")
                stream << "load_simple_spot_lights(" << p.second << ", " << offset << ", device);" << std::endl;
            else
                IG_LOG(L_ERROR) << "Unknown embed class '" << p.first << "'" << std::endl;

            // Special case: Nothing except embedded simple point lights
            if (p.second == counter) {
                stream << "  let finite_lights = e_" << var_name << ";" << std::endl
                       << "  maybe_unused(finite_lights);" << std::endl;
                return stream.str();
            }

            offset += p.second;
        }
    }

    // Write out basic information and start light table
    stream << "  let finite_lights = LightTable {" << std::endl
           << "    count = " << finiteLightCount() << "," << std::endl
           << "    get   = @|id:i32| {" << std::endl;

    bool embedded = false;
    if (isEmbedding()) {
        size_t offset = 0;
        for (const auto& p : mEmbedClassCounter) {
            std::string var_name = to_lowercase(p.first);

            if (offset > 0)
                stream << "    else if ";
            else
                stream << "    if ";

            stream << "id < " << (offset + p.second) << " {" << std::endl
                   << "      e_" << var_name << ".get(id - " << offset << ")" << std::endl
                   << "    }" << std::endl;

            offset += p.second;
            embedded = true;
        }
    }

    if (embedded)
        stream << "    else {" << std::endl;
    stream << "    match(id) {" << std::endl;

    counter = embeddedLightCount();
    for (auto light : mFiniteLights) {
        if (!isEmbedding() || !light->getEmbedClass().has_value())
            stream << "      " << (counter++) << " => light_" << tree.getClosureID(light->name()) << "," << std::endl;
    }

    stream << "      _ => make_null_light(id)" << std::endl;

    if (embedded)
        stream << "    }" << std::endl;

    stream << "    }" << std::endl
           << "  }};" << std::endl
           << "  maybe_unused(finite_lights);" << std::endl;

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
    const auto& lights = ctx.Options.Scene->lights();
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
    setupInfiniteLightIDs(); // Infinite & Finite do not share the same id!
    setupFiniteLightIDs();
    setupAreaLights();
}

void LoaderLight::loadLights(LoaderContext& ctx)
{
    const auto& lights = ctx.Options.Scene->lights();
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

void LoaderLight::setupInfiniteLightIDs()
{
    // Order is based on order in generate
    size_t id = 0;
    for (auto& light : mInfiniteLights)
        light->setID(id++);
}

void LoaderLight::setupFiniteLightIDs()
{
    // Order is based on order in generate
    size_t id = 0;

    const bool embed = isEmbedding();

    // Embedded classes (if necessary)
    if (embed) {
        for (const auto& p : mEmbedClassCounter) {
            const auto embedClass = p.first;

            for (auto& light : mFiniteLights) {
                if (light->getEmbedClass().value_or("") == embedClass)
                    light->setID(id++);
            }
        }
    }

    // Non-embedded lights
    if (embed) {
        for (auto& light : mFiniteLights) {
            if (!light->getEmbedClass().has_value())
                light->setID(id++);
        }
    } else {
        for (auto& light : mFiniteLights)
            light->setID(id++);
    }
}

void LoaderLight::setupAreaLights()
{
    for (const auto& light : mFiniteLights) {
        const auto entity = light->entity();
        if (entity.has_value())
            mAreaLights[entity.value()] = light->id();
    }
}

void LoaderLight::embedLights(ShadingTree& tree)
{
    if (!isEmbedding())
        return;

    for (const auto& p : mEmbedClassCounter) {
        const auto embedClass = p.first;

        // Check if already loaded
        if (tree.context().Database.FixTables.count(embedClass) > 0)
            continue;

        auto& tbl = tree.context().Database.FixTables[embedClass];

        IG_LOG(L_DEBUG) << "Embedding lights of class '" << embedClass << "'" << std::endl;

        for (const auto& light : mFiniteLights) {
            if (light->getEmbedClass().value_or("") != embedClass)
                continue;

            auto& lightData = tbl.addEntry(0);
            VectorSerializer lightSerializer(lightData, false);
            light->embed(Light::EmbedInput{ lightSerializer, tree });
        }
    }
}
std::string LoaderLight::generateLightSelector(std::string type, ShadingTree& tree)
{
    const std::string uniformSelector = "  let light_selector = make_uniform_light_selector(infinite_lights, finite_lights);";

    // If there is just a none or just a single light, do not bother with fancy selectors
    if (lightCount() <= 1)
        return uniformSelector + "\n";

    std::stringstream stream;

    if (type == "hierarchy") {
        auto hierarchy = LightHierarchy::setup(mFiniteLights, tree);
        if (hierarchy.empty()) {
            stream << uniformSelector << std::endl;
        } else {
            stream << "  let light_selector = make_hierarchy_light_selector(infinite_lights, finite_lights, device.load_buffer(\"" << hierarchy.u8string() << "\"));" << std::endl;
        }
    } else if (type == "simple") {
        auto cdf = generateLightSelectionCDF(tree);
        if (cdf.empty()) {
            stream << uniformSelector << std::endl;
        } else {
            stream << "  let light_cdf = cdf::make_cdf_1d_from_buffer(device.load_buffer(\"" << cdf.u8string() << "\"), finite_lights.count, 0);" << std::endl
                   << "  let light_selector = make_cdf_light_selector(infinite_lights, finite_lights, light_cdf);" << std::endl;
        }
    } else { // Default
        stream << uniformSelector << std::endl;
    }

    return stream.str();
}

std::filesystem::path LoaderLight::generateLightSelectionCDF(ShadingTree& tree)
{
    const std::string exported_id = "_light_cdf_";

    const auto data = tree.context().Cache->ExportedData.find(exported_id);
    if (data != tree.context().Cache->ExportedData.end())
        return std::any_cast<std::filesystem::path>(data->second);

    if (lightCount() == 0)
        return {}; // Fallback to null light selector

    const std::filesystem::path path = tree.context().CacheManager->directory() / "light_cdf.bin";

    std::vector<float> estimated_powers;
    estimated_powers.reserve(mFiniteLights.size());
    for (const auto& light : mFiniteLights)
        estimated_powers.push_back(light->computeFlux(tree));

    CDF::computeForArray(estimated_powers, path);

    tree.context().Cache->ExportedData[exported_id] = path;
    return path;
}
} // namespace IG