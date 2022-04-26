#include "LoaderLight.h"
#include "CDF.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"
#include "serialization/VectorSerializer.h"
#include "skysun/SkyModel.h"
#include "skysun/SunLocation.h"

#include <chrono>

// TODO: Make use of the ShadingTree!!
namespace IG {

static ElevationAzimuth extractEA(const std::shared_ptr<Parser::Object>& obj)
{
    if (obj->property("direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj->property("direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj->property("sun_direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj->property("sun_direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj->property("theta").isValid() || obj->property("phi").isValid()) {
        return ElevationAzimuth::fromThetaPhi(obj->property("theta").getNumber(0), obj->property("phi").getNumber(0));
    } else if (obj->property("elevation").isValid() || obj->property("azimuth").isValid()) {
        return ElevationAzimuth{ obj->property("elevation").getNumber(0), obj->property("azimuth").getNumber(0) };
    } else {
        TimePoint timepoint;
        MapLocation location;
        timepoint.Year     = obj->property("year").getInteger(timepoint.Year);
        timepoint.Month    = obj->property("month").getInteger(timepoint.Month);
        timepoint.Day      = obj->property("day").getInteger(timepoint.Day);
        timepoint.Hour     = obj->property("hour").getInteger(timepoint.Hour);
        timepoint.Minute   = obj->property("minute").getInteger(timepoint.Minute);
        timepoint.Seconds  = obj->property("seconds").getNumber(timepoint.Seconds);
        location.Latitude  = obj->property("latitude").getNumber(location.Latitude);
        location.Longitude = obj->property("longitude").getNumber(location.Longitude);
        location.Timezone  = obj->property("timezone").getNumber(location.Timezone);
        return computeSunEA(timepoint, location);
    }
}

static std::string setup_sky(const std::string& name, const std::shared_ptr<Parser::Object>& light)
{
    auto ground    = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
    auto turbidity = light->property("turbidity").getNumber(3.0f);
    auto ea        = extractEA(light);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/skytex_" + ShaderUtils::escapeIdentifier(name) + ".exr";
    SkyModel model(RGB(ground), ea, turbidity);
    model.save(path);
    return path;
}

static std::tuple<std::string, size_t, size_t> setup_cdf(const std::string& filename)
{
    std::string name = std::filesystem::path(filename).stem().generic_u8string();

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/cdf_" + ShaderUtils::escapeIdentifier(name) + ".bin";

    size_t slice_conditional = 0;
    size_t slice_marginal    = 0;
    CDF::computeForImage(filename, path, slice_conditional, slice_marginal, true);

    return { path, slice_conditional, slice_marginal };
}

static void light_point(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    auto pos = light->property("position").getVector3(); // Has to be fixed
    tree.addColor("intensity", *light, Vector3f::Ones(), true, ShadingTree::IM_Light);

    stream << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_point_light(" << ShaderUtils::inlineVector(pos)
           << ", " << tree.getInline("intensity") << ");" << std::endl;

    tree.endClosure();
}

static std::string inline_mat34(const Eigen::Matrix<float, 3, 4>& mat)
{
    std::stringstream stream;
    stream << "make_mat3x4(";
    for (size_t i = 0; i < 4; ++i) {
        stream << "make_vec3(";
        for (size_t j = 0; j < 3; ++j) {
            stream << mat(j, i);
            if (j < 2)
                stream << ", ";
        }
        stream << ")";
        if (i < 3)
            stream << ", ";
    }
    stream << ")";
    return stream.str();
}

static std::string inline_mat3(const Matrix3f& mat)
{
    std::stringstream stream;
    stream << "make_mat3x3(";
    for (size_t i = 0; i < 3; ++i) {
        stream << "make_vec3(";
        for (size_t j = 0; j < 3; ++j) {
            stream << mat(j, i);
            if (j < 2)
                stream << ", ";
        }
        stream << ")";
        if (i < 2)
            stream << ", ";
    }
    stream << ")";
    return stream.str();
}

static std::string inline_entity(const Entity& entity, uint32 shapeID)
{
    const Eigen::Matrix<float, 3, 4> localMat  = entity.Transform.inverse().matrix().block<3, 4>(0, 0);             // To Local
    const Eigen::Matrix<float, 3, 4> globalMat = entity.Transform.matrix().block<3, 4>(0, 0);                       // To Global
    const Matrix3f normalMat                   = entity.Transform.matrix().block<3, 3>(0, 0).transpose().inverse(); // To Global [Normal]

    std::stringstream stream;
    stream << "Entity{ local_mat = " << inline_mat34(localMat)
           << ", global_mat = " << inline_mat34(globalMat)
           << ", normal_mat = " << inline_mat3(normalMat)
           << ", scale = " << std::abs(normalMat.determinant())
           << ", shape_id = " << shapeID << " }";
    return stream.str();
}

inline static bool is_simple_area_light(const std::shared_ptr<Parser::Object>& light)
{
    return light->pluginType() == "area"
           && (!light->property("radiance").isValid() || light->property("radiance").canBeNumber() || light->property("radiance").type() == Parser::PT_VECTOR3)
           && (!light->property("radiance_scale").isValid() || light->property("radiance_scale").canBeNumber() || light->property("radiance_scale").type() == Parser::PT_VECTOR3);
}

inline static Vector3f get_property_color(const Parser::Property& prop, const Vector3f& def)
{
    if (prop.canBeNumber()) {
        return Vector3f::Constant(prop.getNumber());
    } else {
        return prop.getVector3(def);
    }
}

inline static void exportSimpleAreaLights(const LoaderContext& ctx)
{
    // Check if already loaded
    if (ctx.Database->CustomTables.count("SimpleArea") > 0)
        return;

    IG_LOG(L_DEBUG) << "Embedding simple area lights" << std::endl;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light             = pair.second;
        const std::string entityName = light->property("entity").getString();
        const Vector3f radiance      = get_property_color(light->property("radiance"), Vector3f::Zero()).cwiseProduct(get_property_color(light->property("radiance_scale"), Vector3f::Ones()));

        Entity entity;
        if (!ctx.Environment.EmissiveEntities.count(entityName)) {
            IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
            continue;
        } else {
            entity = ctx.Environment.EmissiveEntities.at(entityName);
        }

        const Eigen::Matrix<float, 3, 4> localMat  = entity.Transform.inverse().matrix().block<3, 4>(0, 0);             // To Local
        const Eigen::Matrix<float, 3, 4> globalMat = entity.Transform.matrix().block<3, 4>(0, 0);                       // To Global
        const Matrix3f normalMat                   = entity.Transform.matrix().block<3, 3>(0, 0).inverse().transpose(); // To Global [Normal]
        uint32 shape_id                            = ctx.Environment.ShapeIDs.at(entity.Shape);

        auto& lightData = ctx.Database->CustomTables["SimpleArea"].addLookup(0, 0, DefaultAlignment); // We do not make use of the typeid
        VectorSerializer lightSerializer(lightData, false);
        lightSerializer.write(localMat, true);                           // +3x4 = 12
        lightSerializer.write(globalMat, true);                          // +3x4 = 24
        lightSerializer.write(normalMat, true);                          // +3x3 = 33
        lightSerializer.write((uint32)shape_id);                         // +1   = 34
        lightSerializer.write((float)std::abs(normalMat.determinant())); // +1   = 35
        lightSerializer.write((uint32)0 /*Padding*/);                    // +1   = 36
        lightSerializer.write(radiance);                                 // +3   = 39
    }
}

static void light_area(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    const std::string entityName = light->property("entity").getString();
    tree.addColor("radiance", *light, Vector3f::Constant(1.0f), true, ShadingTree::IM_Light);

    // Not exposed in the documentation, but used internally until we have proper shading nodes
    tree.addColor("radiance_scale", *light, Vector3f::Constant(1.0f), true, ShadingTree::IM_Light);

    Entity entity;
    if (!tree.context().Environment.EmissiveEntities.count(entityName)) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return;
    } else {
        entity = tree.context().Environment.EmissiveEntities.at(entityName);
    }

    uint32 shape_id     = tree.context().Environment.ShapeIDs.at(entity.Shape);
    const auto shape    = tree.context().Environment.Shapes[shape_id];
    size_t shape_offset = tree.context().Database->ShapeTable.lookups()[shape_id].Offset;

    stream << tree.pullHeader()
           << "  let ae_" << ShaderUtils::escapeIdentifier(name) << " = make_shape_area_emitter(" << inline_entity(entity, shape_id)
           << ", device.load_specific_shape(" << shape.FaceCount << ", " << shape.VertexCount << ", " << shape.NormalCount << ", " << shape.TexCount << ", " << shape_offset << ", dtb.shapes));" << std::endl
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_area_light(ae_" << ShaderUtils::escapeIdentifier(name) << ", "
           << " @|tex_coords| { maybe_unused(tex_coords); color_mul(" << tree.getInline("radiance_scale") << ", " << tree.getInline("radiance") << ") });" << std::endl;

    tree.endClosure();
}

static void light_directional(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();
    tree.addColor("irradiance", *light, Vector3f::Ones(), true, ShadingTree::IM_Light);

    stream << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_directional_light(" << ShaderUtils::inlineVector(dir)
           << ", " << ShaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("irradiance") << ");" << std::endl;

    tree.endClosure();
}

static void light_sun(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    auto power      = light->property("sun_scale").getNumber(1.0f);
    auto sun_radius = light->property("sun_radius_scale").getNumber(1.0f);

    stream << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_sun_light(" << ShaderUtils::inlineVector(dir)
           << ", " << ShaderUtils::inlineSceneBBox(tree.context())
           << ", " << sun_radius
           << ", color_mulf(color_builtins::white, " << power << "));" << std::endl;

    tree.endClosure();
}

static void light_sky(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    const std::string path = setup_sky(name, light);
    const auto cdf         = setup_cdf(path);
    const std::string id   = ShaderUtils::escapeIdentifier(name);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    stream << tree.pullHeader()
           << "  let tex_" << id << "   = make_image_texture(make_repeat_border(), make_bilinear_filter(), device.load_image(\"" << path << "\"), mat3x3_identity());" << std::endl // TODO: Refactor this out
           << "  let cdf_" << id << "   = cdf::make_cdf_2d(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
           << "  let light_" << id << " = make_environment_light_textured(" << ShaderUtils::inlineSceneBBox(tree.context())
           << ", tex_" << id << ", cdf_" << id
           << ", " << ShaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static void light_cie_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    tree.addColor("zenith", *light, Vector3f::Ones(), true, ShadingTree::IM_Light);
    tree.addColor("ground", *light, Vector3f::Ones(), true, ShadingTree::IM_Light);
    tree.addNumber("ground_brightness", *light, 0.2f, true, ShadingTree::IM_Light);

    const Matrix3f trans  = light->property("transform").getTransform().linear().transpose().inverse();
    const bool has_ground = light->property("has_ground").getBool(true);

    bool cloudy = (light->pluginType() == "cie_cloudy" || light->pluginType() == "ciecloudy");
    stream << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_cie_sky_light(" << ShaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("zenith")
           << ", " << tree.getInline("ground")
           << ", " << tree.getInline("ground_brightness")
           << ", " << (cloudy ? "true" : "false")
           << ", " << (has_ground ? "true" : "false")
           << ", " << ShaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static inline float perez_model(float zenithAngle, float sunAngle, float a, float b, float c, float d, float e)
{
    float zc = std::cos(zenithAngle);
    float sc = std::cos(sunAngle);

    float A = 1 + a * std::exp(b * zc);
    float B = 1 + c * std::exp(d * sunAngle) + e * sc * sc;
    return A * B;
}

static void light_perez(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    auto a = light->property("a").getNumber(1.0f);
    auto b = light->property("b").getNumber(1.0f);
    auto c = light->property("c").getNumber(1.0f);
    auto d = light->property("d").getNumber(1.0f);
    auto e = light->property("e").getNumber(1.0f);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    Vector3f color;
    if (light->properties().count("luminance")) {
        color = tree.context().extractColor(*light, "luminance");
    } else {
        auto zenith         = tree.context().extractColor(*light, "zenith");
        const float groundZ = perez_model(0, -dir(2), a, b, c, d, e); // TODO: Validate
        color               = zenith * groundZ;
    }

    stream << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_perez_light("
           << ShaderUtils::inlineSceneBBox(tree.context())
           << ", " << ShaderUtils::inlineVector(dir)
           << ", " << ShaderUtils::inlineColor(color)
           << ", " << a
           << ", " << b
           << ", " << c
           << ", " << d
           << ", " << e
           << ", " << ShaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static void light_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    const std::string id = ShaderUtils::escapeIdentifier(name);

    if (light->property("radiance").type() == Parser::PT_STRING) {
        const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

        const std::string tex_name = light->property("radiance").getString();
        const auto tex             = tree.context().Scene.texture(tex_name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << tex_name << "'" << std::endl;
            return; // TODO
        }

        const std::string tex_path = LoaderTexture::getFilename(*tex, tree.context()).generic_u8string();
        if (tex_path.empty()) {
            stream << LoaderTexture::generate(tex_name, *tex, tree)
                   << "  let light_" << id << " = make_environment_light_textured_naive(" << ShaderUtils::inlineSceneBBox(tree.context())
                   << ", tex_" << ShaderUtils::escapeIdentifier(tex_name)
                   << ", " << ShaderUtils::inlineMatrix(trans) << ");" << std::endl;
        } else {
            const auto cdf = setup_cdf(tex_path);
            stream << LoaderTexture::generate(tex_name, *tex, tree)
                   << "  let cdf_" << id << "   = cdf::make_cdf_2d(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                   << "  let light_" << id << " = make_environment_light_textured(" << ShaderUtils::inlineSceneBBox(tree.context())
                   << ", tex_" << ShaderUtils::escapeIdentifier(tex_name)
                   << ", cdf_" << id
                   << ", " << ShaderUtils::inlineMatrix(trans) << ");" << std::endl;
        }
    } else {
        const Vector3f color = tree.context().extractColor(*light, "radiance");
        stream << "  let light_" << id << " = make_environment_light(" << ShaderUtils::inlineSceneBBox(tree.context())
               << ", " << ShaderUtils::inlineColor(color) << ");" << std::endl;
    }

    tree.endClosure();
}

using LightLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, ShadingTree&);
static const struct {
    const char* Name;
    LightLoader Loader;
} _generators[] = {
    { "point", light_point },
    { "area", light_area },
    { "directional", light_directional },
    { "direction", light_directional },
    { "distant", light_directional },
    { "sun", light_sun },
    { "sky", light_sky },
    { "cie_uniform", light_cie_env },
    { "cieuniform", light_cie_env },
    { "cie_cloudy", light_cie_env },
    { "ciecloudy", light_cie_env },
    { "perez", light_perez },
    { "uniform", light_env },
    { "env", light_env },
    { "envmap", light_env },
    { "constant", light_env },
    { "", nullptr }
};

std::string LoaderLight::generate(ShadingTree& tree, bool skipArea)
{
    constexpr size_t MaxSimpleInlineAreaLights = 10;

    size_t simpleAreaLightCounter = 0;
    if (!skipArea) {
        for (const auto& pair : tree.context().Scene.lights()) {
            const auto light = pair.second;

            if (is_simple_area_light(light))
                ++simpleAreaLightCounter;
        }
    }

    const bool embedSimpleAreaLights = simpleAreaLightCounter > MaxSimpleInlineAreaLights;

    // Export simple area lights
    if (embedSimpleAreaLights)
        exportSimpleAreaLights(tree.context());

    // This will be used for now
    auto skip = [&](const std::shared_ptr<Parser::Object>& light) { return skipArea && light->pluginType() == "area"; };

    std::stringstream stream;

    size_t counter = embedSimpleAreaLights ? simpleAreaLightCounter : 0;
    for (const auto& pair : tree.context().Scene.lights()) {
        const auto light = pair.second;

        if (skip(light) || (embedSimpleAreaLights && is_simple_area_light(light)))
            continue;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == light->pluginType()) {
                _generators[i].Loader(stream, pair.first, light, tree);
                ++counter;
                found = true;
                break;
            }
        }
        if (!found)
            IG_LOG(L_ERROR) << "No light type '" << light->pluginType() << "' available" << std::endl;
    }

    if (counter != 0)
        stream << std::endl;

    if (embedSimpleAreaLights) {
        stream << "  let simple_area_lights = load_simple_area_lights(device, shapes);" << std::endl;

        // Special case: Nothing except embedded simple area lights
        if (counter > 0 && counter == simpleAreaLightCounter) {
            stream << "  let num_lights = " << counter << ";" << std::endl
                   << "  let lights = simple_area_lights;" << std::endl;
            return stream.str();
        }
    }

    stream << "  let num_lights = " << counter << ";" << std::endl
           << "  let lights = @|id:i32| {" << std::endl
           << "    match(id) {" << std::endl;

    // If embedding simple area light, we collect them all at the "else" case
    // If not embedding, the last entry will be the "else" case
    size_t counter2 = 0;
    for (const auto& pair : tree.context().Scene.lights()) {
        const auto light = pair.second;

        if (skip(light))
            continue;

        if (!embedSimpleAreaLights || !is_simple_area_light(light)) {
            if (embedSimpleAreaLights || counter2 < counter - 1)
                stream << "      " << counter2;
            else
                stream << "      _";

            stream << " => light_" << ShaderUtils::escapeIdentifier(pair.first)
                   << "," << std::endl;
        }
        ++counter2;
    }

    if (counter2 == 0) {
        if (!skipArea) // Don't trigger a warning if we skip areas
            IG_LOG(L_WARNING) << "Scene does not contain lights" << std::endl;
        stream << "      _ => make_null_light()" << std::endl;
    } else if (embedSimpleAreaLights) {
        stream << "      _ => simple_area_lights(id)" << std::endl;
    }

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}

void LoaderLight::setupAreaLights(LoaderContext& ctx)
{
    uint32 counter = 0;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;

        if (light->pluginType() == "area") {
            const std::string entity              = light->property("entity").getString();
            ctx.Environment.AreaLightsMap[entity] = counter;
        }

        ++counter;
    }
}

bool LoaderLight::hasAreaLights(const LoaderContext& ctx)
{
    return !ctx.Environment.AreaLightsMap.empty();
}
} // namespace IG