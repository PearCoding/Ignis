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
    SkyModel model(ground, ea, turbidity);
    model.save(path);
    return path;
}

static std::tuple<std::string, size_t, size_t> setup_cdf(const std::string& filename)
{
    std::string name = std::filesystem::path(filename).stem();

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/cdf_" + ShaderUtils::escapeIdentifier(name) + ".bin";

    size_t slice_conditional;
    size_t slice_marginal;
    CDF::computeForImage(filename, path, slice_conditional, slice_marginal, true);

    return { path, slice_conditional, slice_marginal };
}

static void light_point(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    auto pos       = light->property("position").getVector3();
    auto intensity = ctx.extractColor(*light, "intensity");

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_point_light(" << ShaderUtils::inlineVector(pos)
           << ", " << ShaderUtils::inlineColor(intensity) << ");" << std::endl;
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
           << ", shape_id = " << shapeID << " }";
    return stream.str();
}

static void light_area(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    ShadingTree tree(name);
    tree.onlyUseCoords(true);

    const std::string entityName = light->property("entity").getString();
    tree.addColor("radiance", ctx, *light, Vector3f::Constant(1.0f));

    // Not exposed in the documentation, but used internally until we have proper shading nodes
    tree.addColor("radiance_scale", ctx, *light, Vector3f::Constant(1.0f));

    uint32 entity_id = 0;
    if (!ctx.Environment.EntityIDs.count(entityName))
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
    else
        entity_id = ctx.Environment.EntityIDs.at(entityName);

    const auto entity   = ctx.Environment.Entities[entity_id];
    uint32 shape_id     = ctx.Environment.ShapeIDs.at(entity.Shape);
    const auto shape    = ctx.Environment.Shapes[shape_id];
    size_t shape_offset = ctx.Database->ShapeTable.lookups()[shape_id].Offset;

    stream << "  let ae_" << ShaderUtils::escapeIdentifier(name) << " = make_shape_area_emitter(" << inline_entity(entity, shape_id)
           << ", device.load_specific_shape(" << shape.FaceCount << ", " << shape.VertexCount << ", " << shape.NormalCount << ", " << shape.TexCount << ", " << shape_offset << ", dtb.shapes));" << std::endl
           << tree.pullHeader()
           << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_area_light(ae_" << ShaderUtils::escapeIdentifier(name) << ", "
           << " @|tex_coords| { maybe_unused(tex_coords); color_mul(" << tree.getInline("radiance_scale") << ", " << tree.getInline("radiance") << ") });" << std::endl;
}

static void light_directional(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    IG_UNUSED(name);

    auto ea         = extractEA(light);
    Vector3f dir    = ea.toDirection();
    auto irradiance = ctx.extractColor(*light, "irradiance");

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_directional_light(" << ShaderUtils::inlineVector(dir)
           << ", " << ctx.Environment.SceneDiameter / 2
           << ", " << ShaderUtils::inlineColor(irradiance) << ");" << std::endl;
}

static void light_sun(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    auto power      = light->property("sun_scale").getNumber(1.0f);
    auto sun_radius = light->property("sun_radius_scale").getNumber(1.0f);

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_sun_light(" << ShaderUtils::inlineVector(dir)
           << ", " << ctx.Environment.SceneDiameter / 2
           << ", " << sun_radius
           << ", color_mulf(white, " << power << "));" << std::endl;
}

static void light_sky(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    const std::string path = setup_sky(name, light);
    const auto cdf         = setup_cdf(path);
    const std::string id   = ShaderUtils::escapeIdentifier(name);

    stream << "  let tex_" << id << "   = make_image_texture(make_repeat_border(), make_bilinear_filter(), device.load_image(\"" << path << "\"), false, false);" << std::endl
           << "  let cdf_" << id << "   = cdf::make_cdf_2d(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
           << "  let light_" << id << " = make_environment_light_textured(" << ctx.Environment.SceneDiameter / 2
           << ", tex_" << id << ", cdf_" << id << ", 0, 0);" << std::endl;
}

static void light_cie_uniform(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    IG_UNUSED(name);

    auto zenith           = ctx.extractColor(*light, "zenith");
    auto ground           = ctx.extractColor(*light, "ground");
    auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_cie_sky_light(" << ctx.Environment.SceneDiameter / 2
           << ", " << ShaderUtils::inlineColor(zenith)
           << ", " << ShaderUtils::inlineColor(ground)
           << ", " << groundbrightness
           << ", false);" << std::endl;
}

static void light_cie_cloudy(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    IG_UNUSED(name);

    auto zenith           = ctx.extractColor(*light, "zenith");
    auto ground           = ctx.extractColor(*light, "ground");
    auto groundbrightness = light->property("ground_brightness").getNumber(0.2f);

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_cie_sky_light(" << ctx.Environment.SceneDiameter / 2
           << ", " << ShaderUtils::inlineColor(zenith)
           << ", " << ShaderUtils::inlineColor(ground)
           << ", " << groundbrightness
           << ", false);" << std::endl;
}

static inline float perez_model(float zenithAngle, float sunAngle, float a, float b, float c, float d, float e)
{
    float zc = std::cos(zenithAngle);
    float sc = std::cos(sunAngle);

    float A = 1 + a * std::exp(b * zc);
    float B = 1 + c * std::exp(d * sunAngle) + e * sc * sc;
    return A * B;
}

static void light_perez(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    IG_UNUSED(name);

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    auto a = light->property("a").getNumber(1.0f);
    auto b = light->property("b").getNumber(1.0f);
    auto c = light->property("c").getNumber(1.0f);
    auto d = light->property("d").getNumber(1.0f);
    auto e = light->property("e").getNumber(1.0f);

    Vector3f color;
    if (light->properties().count("luminance")) {
        color = ctx.extractColor(*light, "luminance");
    } else {
        auto zenith         = ctx.extractColor(*light, "zenith");
        const float groundZ = perez_model(0, -dir(2), a, b, c, d, e); // TODO: Validate
        color               = zenith * groundZ;
    }

    stream << "  let light_" << ShaderUtils::escapeIdentifier(name) << " = make_perez_light(" << ctx.Environment.SceneDiameter / 2
           << ", " << ShaderUtils::inlineVector(dir)
           << ", " << ShaderUtils::inlineColor(color)
           << ", " << a
           << ", " << b
           << ", " << c
           << ", " << d
           << ", " << e << ");" << std::endl;
}

static void light_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    IG_UNUSED(name);

    const std::string id = ShaderUtils::escapeIdentifier(name);

    if (light->property("radiance").type() == Parser::PT_STRING) {
        float theta_off = light->property("theta").getNumber(0.0f) * Deg2Rad;
        float phi_off   = light->property("phi").getNumber(0.0f) * Deg2Rad;

        const std::string tex_name = light->property("radiance").getString();
        const auto tex             = ctx.Scene.texture(tex_name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << tex_name << "'" << std::endl;
            return; //TODO
        }

        const std::string tex_path = LoaderTexture::getFilename(*tex, ctx);
        if (tex_path.empty()) {
            ShadingTree tree;
            stream << LoaderTexture::generate(tex_name, *tex, ctx, tree)
                   << "  let light_" << id << " = make_environment_light_textured_naive(" << ctx.Environment.SceneDiameter / 2
                   << ", tex_" << ShaderUtils::escapeIdentifier(tex_name) << ", "
                   << theta_off << ", " << phi_off << ");" << std::endl;
        } else {
            const auto cdf = setup_cdf(tex_path);
            ShadingTree tree;
            stream << LoaderTexture::generate(tex_name, *tex, ctx, tree)
                   << "  let cdf_" << id << "   = cdf::make_cdf_2d(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                   << "  let light_" << id << " = make_environment_light_textured(" << ctx.Environment.SceneDiameter / 2
                   << ", tex_" << ShaderUtils::escapeIdentifier(tex_name)
                   << ", cdf_" << id << ", "
                   << theta_off << ", " << phi_off << ");" << std::endl;
        }
    } else {
        const Vector3f color = ctx.extractColor(*light, "radiance");
        stream << "  let light_" << id << " = make_environment_light(" << ctx.Environment.SceneDiameter / 2
               << ", " << ShaderUtils::inlineColor(color) << ");" << std::endl;
    }
}

using LightLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);
static struct {
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
    { "cie_uniform", light_cie_uniform },
    { "cieuniform", light_cie_uniform },
    { "cie_cloudy", light_cie_cloudy },
    { "ciecloudy", light_cie_cloudy },
    { "perez", light_perez },
    { "uniform", light_env },
    { "env", light_env },
    { "envmap", light_env },
    { "constant", light_env },
    { "", nullptr }
};

std::string LoaderLight::generate(const LoaderContext& ctx, bool skipArea)
{
    // This will be used for now
    auto skip = [&](const std::string& type) { return (skipArea && type == "area"); };

    std::stringstream stream;

    size_t counter = 0;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;

        if (skip(light->pluginType()))
            continue;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == light->pluginType()) {
                _generators[i].Loader(stream, pair.first, light, ctx);
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

    stream << "  let num_lights = " << counter << ";" << std::endl
           << "  let lights = @|id:i32| {" << std::endl
           << "    match(id) {" << std::endl;

    size_t counter2 = 0;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;

        if (skip(light->pluginType()))
            continue;

        if (counter2 < counter - 1)
            stream << "      " << counter2;
        else
            stream << "      _";

        stream << " => light_" << ShaderUtils::escapeIdentifier(pair.first)
               << "," << std::endl;
        ++counter2;
    }

    if (counter == 0) {
        if (!skipArea) // Don't trigger a warning if we skip areas
            IG_LOG(L_WARNING) << "Scene does not contain lights" << std::endl;
        stream << "    _ => make_null_light()" << std::endl;
    }

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}

void LoaderLight::setupAreaLights(LoaderContext& ctx)
{
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;

        if (light->pluginType() != "area")
            continue;

        const std::string entity              = light->property("entity").getString();
        ctx.Environment.AreaLightsMap[entity] = pair.first;
    }
}

bool LoaderLight::hasAreaLights(const LoaderContext& ctx)
{
    return !ctx.Environment.AreaLightsMap.empty();
}
} // namespace IG