#include "LoaderLight.h"
#include "CDF.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "serialization/VectorSerializer.h"
#include "skysun/SkyModel.h"
#include "skysun/SunLocation.h"

#include <algorithm>
#include <chrono>

namespace IG {
inline static Vector3f getPropertyColor(const Parser::Property& prop, const Vector3f& def)
{
    if (prop.canBeNumber()) {
        return Vector3f::Constant(prop.getNumber());
    } else {
        return prop.getVector3(def);
    }
}

inline static float estimatePower(const Parser::Property& prop, const Vector3f& def)
{
    const Vector3f color = getPropertyColor(prop, def);
    return color.mean(); // Do not use the luminance, as we do not know if this is perceptional important
}

static ElevationAzimuth extractEA(const std::shared_ptr<Parser::Object>& obj)
{
    if (obj->property("direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj->property("direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj->property("sun_direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj->property("sun_direction").getVector3(Vector3f(0, 0, 1)).normalized());
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

static std::string setup_sky(LoaderContext& ctx, const std::string& name, const std::shared_ptr<Parser::Object>& light)
{
    const std::string exported_id = "_sky_" + name;

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<std::string>(data->second);

    auto ground    = light->property("ground").getVector3(Vector3f(0.8f, 0.8f, 0.8f));
    auto turbidity = light->property("turbidity").getNumber(3.0f);
    auto ea        = extractEA(light);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/skytex_" + LoaderUtils::escapeIdentifier(name) + ".exr";
    SkyModel model(RGB(ground), ea, turbidity);
    model.save(path);

    ctx.ExportedData[exported_id] = path;
    return path;
}

using CDFData = std::tuple<std::string, size_t, size_t>;
static CDFData setup_cdf(LoaderContext& ctx, const std::string& filename)
{
    const std::string exported_id = "_cdf_" + filename;

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<CDFData>(data->second);

    std::string name = std::filesystem::path(filename).stem().generic_u8string();

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/cdf_" + LoaderUtils::escapeIdentifier(name) + ".bin";

    size_t slice_conditional = 0;
    size_t slice_marginal    = 0;
    CDF::computeForImage(filename, path, slice_conditional, slice_marginal, true);

    const CDFData cdf_data        = { path, slice_conditional, slice_marginal };
    ctx.ExportedData[exported_id] = cdf_data;
    return cdf_data;
}

inline static bool is_simple_point_light(const std::shared_ptr<Parser::Object>& light)
{
    const auto position  = light->property("position");
    const auto intensity = light->property("intensity");
    return light->pluginType() == "point"
           && (!position.isValid() || position.type() == Parser::PT_VECTOR3)
           && (!intensity.isValid() || intensity.canBeNumber() || intensity.type() == Parser::PT_VECTOR3);
}

inline static void exportSimplePointLights(const LoaderContext& ctx)
{
    // Check if already loaded
    if (ctx.Database->CustomTables.count("SimplePoint") > 0)
        return;

    IG_LOG(L_DEBUG) << "Embedding simple point lights" << std::endl;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;
        if (light->pluginType() != "point")
            continue;

        const Vector3f position = light->property("position").getVector3();
        const Vector3f radiance = getPropertyColor(light->property("intensity"), Vector3f::Zero());

        auto& lightData = ctx.Database->CustomTables["SimplePoint"].addLookup(0, 0, 0); // We do not make use of the typeid
        VectorSerializer lightSerializer(lightData, false);
        lightSerializer.write(position);              // +3   = 3
        lightSerializer.write((uint32)0 /*Padding*/); // +1   = 4
        lightSerializer.write(radiance);              // +3   = 7
        lightSerializer.write((uint32)0 /*Padding*/); // +1   = 8
    }
}

static void light_point(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);
    auto pos = light->property("position").getVector3(); // FIXME: Has to be fixed (WHY?)
    tree.addColor("intensity", *light, Vector3f::Ones(), true);

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_point_light(" << id
           << ", " << LoaderUtils::inlineVector(pos)
           << ", " << tree.getInline("intensity") << ");" << std::endl;

    tree.endClosure();
}

static float light_point_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext&)
{
    return estimatePower(light->property("intensity"), Vector3f::Ones()) * 4 * Pi;
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
    const auto radiance = light->property("radiance");
    return light->pluginType() == "area"
           && (!radiance.isValid() || radiance.canBeNumber() || radiance.type() == Parser::PT_VECTOR3);
}

inline static void exportSimpleAreaLights(const LoaderContext& ctx)
{
    // Check if already loaded
    if (ctx.Database->CustomTables.count("SimpleArea") > 0)
        return;

    IG_LOG(L_DEBUG) << "Embedding simple area lights" << std::endl;
    for (const auto& pair : ctx.Scene.lights()) {
        const auto light = pair.second;
        if (light->pluginType() != "area")
            continue;

        const std::string entityName = light->property("entity").getString();
        const Vector3f radiance      = getPropertyColor(light->property("radiance"), Vector3f::Zero());

        Entity entity;
        if (!ctx.Environment.EmissiveEntities.count(entityName)) {
            IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
            continue;
        } else {
            entity = ctx.Environment.EmissiveEntities.at(entityName);
        }

        const Eigen::Matrix<float, 3, 4> localMat  = entity.computeLocalMatrix();        // To Local
        const Eigen::Matrix<float, 3, 4> globalMat = entity.computeGlobalMatrix();       // To Global
        const Matrix3f normalMat                   = entity.computeGlobalNormalMatrix(); // To Global [Normal]
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

static void light_area(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);

    const std::string entityName = light->property("entity").getString();
    tree.addColor("radiance", *light, Vector3f::Constant(1.0f), true);

    Entity entity;
    if (!tree.context().Environment.EmissiveEntities.count(entityName)) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return;
    } else {
        entity = tree.context().Environment.EmissiveEntities.at(entityName);
    }

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader();

    const bool opt = light->property("optimize").getBool(true);

    uint32 shape_id = tree.context().Environment.ShapeIDs.at(entity.Shape);
    if (opt && tree.context().Environment.PlaneShapes.count(shape_id) > 0) {
        // Use more specialized area light
        const auto& shape = tree.context().Environment.PlaneShapes.at(shape_id);
        Vector3f origin   = entity.Transform * shape.Origin;
        Vector3f x_axis   = entity.Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity.Transform.linear() * shape.YAxis;
        Vector3f normal   = x_axis.cross(y_axis).normalized();
        float area        = x_axis.cross(y_axis).norm();

        stream << "  let ae_" << light_id << " = make_plane_area_emitter(" << LoaderUtils::inlineVector(origin)
               << ", " << LoaderUtils::inlineVector(x_axis)
               << ", " << LoaderUtils::inlineVector(y_axis)
               << ", " << LoaderUtils::inlineVector(normal)
               << ", " << area
               << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[0])
               << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[1])
               << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[2])
               << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[3])
               << ");" << std::endl;
    } else {
        const auto shape    = tree.context().Environment.Shapes[shape_id];
        size_t shape_offset = tree.context().Database->ShapeTable.lookups()[shape_id].Offset;

        stream << "  let ae_" << light_id << " = make_shape_area_emitter(" << inline_entity(entity, shape_id)
               << ", device.load_specific_shape(" << shape.FaceCount << ", " << shape.VertexCount << ", " << shape.NormalCount << ", " << shape.TexCount << ", " << shape_offset << ", dtb.shapes));" << std::endl;
    }

    stream << "  let light_" << light_id << " = make_area_light(" << id
           << ", ae_" << light_id
           << ", @|ctx| { maybe_unused(ctx); " << tree.getInline("radiance") << " });" << std::endl;

    tree.endClosure();
}

static float light_area_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    const float power = estimatePower(light->property("radiance"), Vector3f::Ones());

    const std::string entityName = light->property("entity").getString();
    Entity entity;
    if (!ctx.Environment.EmissiveEntities.count(entityName)) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return 0;
    } else {
        entity = ctx.Environment.EmissiveEntities.at(entityName);
    }

    uint32 shape_id  = ctx.Environment.ShapeIDs.at(entity.Shape);
    const auto shape = ctx.Environment.Shapes[shape_id];

    const float approx_area = shape.Area * std::abs(entity.computeGlobalMatrix().block<3, 3>(0, 0).determinant());
    return power * approx_area;
}

static void light_directional(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();
    tree.addColor("irradiance", *light, Vector3f::Ones(), true);

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_directional_light(" << id
           << ", " << LoaderUtils::inlineVector(dir)
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("irradiance") << ");" << std::endl;

    tree.endClosure();
}

static float light_directional_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    const float power = estimatePower(light->property("irradiance"), Vector3f::Ones());
    return power * Pi * ctx.Environment.SceneDiameter * ctx.Environment.SceneDiameter / 4;
}

static void light_spot(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();
    auto pos     = light->property("position").getVector3(); // FIXME: Has to be fixed (WHY?)

    tree.addColor("intensity", *light, Vector3f::Ones(), true);
    tree.addNumber("cutoff", *light, 30, true);
    tree.addNumber("falloff", *light, 20, true);

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_spot_light(" << id
           << ", " << LoaderUtils::inlineVector(pos)
           << ", " << LoaderUtils::inlineVector(dir)
           << ", rad(" << tree.getInline("cutoff") << ")"
           << ", rad(" << tree.getInline("falloff") << ")"
           << ", " << tree.getInline("intensity") << ");" << std::endl;

    tree.endClosure();
}

static float light_spot_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext&)
{
    const float power   = estimatePower(light->property("intensity"), Vector3f::Ones());
    const float cutoff  = light->property("cutoff").getNumber(30) * Deg2Rad;
    const float falloff = light->property("falloff").getNumber(20) * Deg2Rad;
    return power * 2 * Pi * (1 - 0.5f * (std::cos(cutoff) + std::cos(falloff)));
}

static void light_sun(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);
    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    tree.addNumber("sun_scale", *light, 1, true);
    tree.addNumber("sun_radius_scale", *light, 1, true);

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_sun_light(" << id
           << ", " << LoaderUtils::inlineVector(dir)
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("sun_radius_scale")
           << ", color_mulf(color_builtins::white, " << tree.getInline("sun_scale") << "));" << std::endl;

    tree.endClosure();
}

static float light_sun_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext&)
{
    const float sun_scale        = light->property("sun_scale").getNumber(1);
    const float sun_radius_scale = light->property("sun_radius_scale").getNumber(1);
    return sun_scale * Pi * sun_radius_scale;
}

static void light_sky(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);
    tree.addColor("scale", *light, Vector3f::Ones(), true);

    const std::string path = setup_sky(tree.context(), name, light);
    const auto cdf         = setup_cdf(tree.context(), path);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    size_t res_img_id          = tree.context().registerExternalResource(path);
    size_t res_cdf_id          = tree.context().registerExternalResource(std::get<0>(cdf));
    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let sky_tex_" << light_id << " = make_image_texture(make_repeat_border(), make_bilinear_filter(), device.load_image_by_id(" << res_img_id << "), mat3x3_identity());" << std::endl // TODO: Refactor this out
           << "  let sky_cdf_" << light_id << " = cdf::make_cdf_2d_from_buffer(device.load_buffer_by_id(" << res_cdf_id << "), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
           << "  let light_" << light_id << "   = make_environment_light_textured(" << id
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("scale")
           << ", sky_tex_" << light_id << ", sky_cdf_" << light_id
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static float light_sky_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    // TODO: Better approximation?
    const float scale = estimatePower(light->property("scale"), Vector3f::Ones());
    return scale * Pi * ctx.Environment.SceneDiameter * ctx.Environment.SceneDiameter / 4;
}

static void light_cie_env(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);

    tree.addColor("zenith", *light, Vector3f::Ones(), true);
    tree.addColor("ground", *light, Vector3f::Ones(), true);
    tree.addNumber("ground_brightness", *light, 0.2f, true);

    const Matrix3f trans  = light->property("transform").getTransform().linear().transpose().inverse();
    const bool has_ground = light->property("has_ground").getBool(true);

    bool cloudy = (light->pluginType() == "cie_cloudy" || light->pluginType() == "ciecloudy");

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_cie_sky_light(" << id
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("zenith")
           << ", " << tree.getInline("ground")
           << ", " << tree.getInline("ground_brightness")
           << ", " << (cloudy ? "true" : "false")
           << ", " << (has_ground ? "true" : "false")
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static float light_cie_env_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    // TODO: Better approximation?
    const float zenith = estimatePower(light->property("zenith"), Vector3f::Ones());
    return zenith * Pi * ctx.Environment.SceneDiameter * ctx.Environment.SceneDiameter / 4;
}

inline float skylight_normalization_factor(float altitude, bool clear)
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

static void light_cie_sunny_env(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    const bool clear = (light->pluginType() == "cie_clear" || light->pluginType() == "cieclear");

    tree.beginClosure(name);

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    if (ea.Elevation > 87 * Deg2Rad) {
        IG_LOG(L_WARNING) << " Sun too close to zenith, reducing elevation to 87 degrees" << std::endl;
        ea.Elevation = 87 * Deg2Rad;
    }

    const float turbidity = light->property("turbidity").getNumber(2.45f);

    constexpr float SkyIllum = 203;
    float zenithbrightness   = (1.376f * turbidity - 1.81f) * std::tan(ea.Elevation) + 0.38f;
    if (!clear)
        zenithbrightness = (zenithbrightness + 8.6f * dir.y() + 0.123f) / 2;
    zenithbrightness = std::max(0.0f, zenithbrightness * 1000 / SkyIllum);

    float factor = 0;
    if (clear)
        factor = 0.274f * (0.91f + 10 * std::exp(-3 * (Pi2 - ea.Elevation)) + 0.45f * dir.y() * dir.y());
    else
        factor = (2.739f + 0.9891f * std::sin(0.3119f + 2.6f * ea.Elevation)) * std::exp(-(Pi2 - ea.Elevation) * (0.4441f + 1.48f * ea.Elevation));

    const float norm_factor = skylight_normalization_factor(ea.Elevation, clear) * InvPi / factor;

    constexpr float SunIllum    = 208;
    const float solarbrightness = 1.5e9f / SunIllum * (1.147f - 0.147f / std::max(dir.y(), 0.16f));
    const float additive_factor = 6e-5f * InvPi * solarbrightness * dir.y() * (clear ? 1 : 0.15f /* Fudge factor */);

    const float c2 = zenithbrightness * norm_factor + additive_factor;

    tree.addColor("scale", *light, Vector3f::Ones(), true);
    tree.addColor("zenith", *light, Vector3f::Ones(), true);
    tree.addColor("ground", *light, Vector3f::Ones(), true);
    tree.addNumber("ground_brightness", *light, 0.2f, true);

    const Matrix3f trans  = light->property("transform").getTransform().linear().transpose().inverse();
    const bool has_ground = light->property("has_ground").getBool(true);

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_cie_sunny_light(" << id
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("scale")
           << ", " << tree.getInline("zenith")
           << ", " << zenithbrightness
           << ", " << tree.getInline("ground")
           << ", " << tree.getInline("ground_brightness")
           << ", " << (clear ? "true" : "false")
           << ", " << (has_ground ? "true" : "false")
           << ", " << LoaderUtils::inlineVector(dir)
           << ", " << factor
           << ", " << c2
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static void light_perez(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    tree.addNumber("a", *light, 1.0f, true);
    tree.addNumber("b", *light, 1.0f, true);
    tree.addNumber("c", *light, 1.0f, true);
    tree.addNumber("d", *light, 1.0f, true);
    tree.addNumber("e", *light, 1.0f, true);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    bool usesLuminance = false;
    if (light->properties().count("luminance")) {
        tree.addColor("luminance", *light, Vector3f::Ones(), true);
        usesLuminance = true;
    } else {
        tree.addColor("zenith", *light, Vector3f::Ones(), true);
        usesLuminance = false;
    }

    const std::string light_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let light_" << light_id << " = make_perez_light(" << id
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << LoaderUtils::inlineVector(dir);

    if (usesLuminance) {
        stream << ", " << tree.getInline("luminance");
    } else {
        stream << ", color_mulf(" << tree.getInline("zenith")
               << ", calc_perez(" << std::min(1.0f, std::max(-1.0f, -dir(1)))
               << ", 1, " << tree.getInline("a")
               << ", " << tree.getInline("b")
               << ", " << tree.getInline("c")
               << ", " << tree.getInline("d")
               << ", " << tree.getInline("e")
               << "))";
    }

    stream << ", " << tree.getInline("a")
           << ", " << tree.getInline("b")
           << ", " << tree.getInline("c")
           << ", " << tree.getInline("d")
           << ", " << tree.getInline("e")
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static void light_env(size_t id, std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure(name);
    tree.addColor("scale", *light, Vector3f::Ones(), true);
    tree.addTexture("radiance", *light, true);
    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    const bool useCDF = light->property("cdf").getBool(true);

    const std::string light_id = tree.currentClosureID();
    if (tree.isPureTexture("radiance")) {
        const std::string tex_name = light->property("radiance").getString();
        const auto tex             = tree.context().Scene.texture(tex_name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << tex_name << "'" << std::endl;
            tree.signalError();
            return; // TODO
        }

        const std::string tex_path = LoaderTexture::getFilename(*tex, tree.context()).generic_u8string();
        if (!useCDF || tex_path.empty()) {
            stream << tree.pullHeader()
                   << "  let light_" << light_id << " = make_environment_light(" << id
                   << ", " << LoaderUtils::inlineSceneBBox(tree.context())
                   << ", " << tree.getInline("scale")
                   << ", tex_" << tree.getClosureID(tex_name)
                   << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        } else {
            const auto cdf          = setup_cdf(tree.context(), tex_path);
            const size_t res_cdf_id = tree.context().registerExternalResource(std::get<0>(cdf));
            stream << tree.pullHeader()
                   << "  let cdf_" << light_id << "   = cdf::make_cdf_2d_from_buffer(device.load_buffer_by_id(" << res_cdf_id << "), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                   << "  let light_" << light_id << " = make_environment_light_textured(" << id
                   << ", " << LoaderUtils::inlineSceneBBox(tree.context())
                   << ", " << tree.getInline("scale")
                   << ", tex_" << tree.getClosureID(tex_name)
                   << ", cdf_" << light_id
                   << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        }
    } else {
        stream << tree.pullHeader()
               << "  let light_" << light_id << " = make_environment_light(" << id
               << ", " << LoaderUtils::inlineSceneBBox(tree.context())
               << ", " << tree.getInline("scale")
               << ", " << tree.getInline("radiance")
               << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
    }

    tree.endClosure();
}

static float light_env_power(const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx)
{
    // TODO: Better approximation?
    const float scale    = estimatePower(light->property("scale"), Vector3f::Ones());
    const float radiance = estimatePower(light->property("radiance"), Vector3f::Ones());
    return scale * radiance * Pi * ctx.Environment.SceneDiameter * ctx.Environment.SceneDiameter / 4;
}

static void light_error(std::ostream& stream, const std::string& name, ShadingTree& tree)
{
    tree.beginClosure(name);
    const std::string light_id = tree.currentClosureID();
    stream << "  let light_" << light_id << " = make_null_light(-1) /* Error */;" << std::endl;
    tree.endClosure();
}

using LightLoader   = void (*)(size_t id, std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, ShadingTree&);
using EstimatePower = float (*)(const std::shared_ptr<Parser::Object>&, const LoaderContext&);
static const struct {
    const char* Name;
    LightLoader Loader;
    EstimatePower Power;
} _generators[] = {
    { "point", light_point, light_point_power },
    { "area", light_area, light_area_power },
    { "directional", light_directional, light_directional_power },
    { "direction", light_directional, light_directional_power },
    { "distant", light_directional, light_directional_power },
    { "spot", light_spot, light_spot_power },
    { "sun", light_sun, light_sun_power },
    { "sky", light_sky, light_sky_power },
    { "cie_uniform", light_cie_env, light_cie_env_power },
    { "cieuniform", light_cie_env, light_cie_env_power },
    { "cie_cloudy", light_cie_env, light_cie_env_power },
    { "ciecloudy", light_cie_env, light_cie_env_power },
    { "cie_clear", light_cie_sunny_env, light_cie_env_power },
    { "cieclear", light_cie_sunny_env, light_cie_env_power },
    { "cie_intermediate", light_cie_sunny_env, light_cie_env_power },
    { "cieintermediate", light_cie_sunny_env, light_cie_env_power },
    { "perez", light_perez, light_cie_env_power },
    { "uniform", light_env, light_env_power },
    { "env", light_env, light_env_power },
    { "envmap", light_env, light_env_power },
    { "constant", light_env, light_env_power },
    { "", nullptr, nullptr }
};

std::string LoaderLight::generate(ShadingTree& tree, bool skipArea)
{
    // Embed lights if necessary. This is cached for multiple calls
    embedLights(tree.context());

    const bool embedSimplePointLights = mSimplePointLightCounter > 0;
    const bool embedSimpleAreaLights  = mSimpleAreaLightCounter > 0;

    // This will be used for now
    auto skip = [&](const std::shared_ptr<Parser::Object>& light) { return skipArea && light->pluginType() == "area"; };

    std::stringstream stream;

    // Write all non-embedded lights to shader
    const auto non_e_it = mOrderedLights.begin() + (mSimplePointLightCounter + mSimpleAreaLightCounter);
    size_t counter      = mSimplePointLightCounter + mSimpleAreaLightCounter;
    for (auto it = non_e_it; it != mOrderedLights.end(); ++it, ++counter) {
        const auto light = it->second;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == light->pluginType()) {
                if (!skip(light))
                    _generators[i].Loader(counter, stream, it->first, light, tree);
                found = true;
                break;
            }
        }

        if (!found) {
            IG_LOG(L_ERROR) << "No light type '" << light->pluginType() << "' available" << std::endl;
            light_error(stream, it->first, tree);
        }
    }

    // Add a new line for cosmetics if necessary :P
    if (!mOrderedLights.empty())
        stream << std::endl;

    // Load embedded point lights if necessary
    if (embedSimplePointLights) {
        stream << "  let simple_point_lights = load_simple_point_lights(0, device);" << std::endl;

        // Special case: Nothing except embedded simple point lights
        if (mOrderedLights.size() == mSimplePointLightCounter) {
            stream << "  let num_lights = " << mOrderedLights.size() << ";" << std::endl
                   << "  let lights = simple_point_lights;" << std::endl;
            return stream.str();
        }
    }

    // Load embedded area lights if necessary
    if (!skipArea && embedSimpleAreaLights) {
        stream << "  let simple_area_lights = load_simple_area_lights(" << mSimplePointLightCounter << ", device, shapes);" << std::endl;

        // Special case: Nothing except embedded simple area lights
        if (mOrderedLights.size() == mSimpleAreaLightCounter) {
            stream << "  let num_lights = " << mOrderedLights.size() << ";" << std::endl
                   << "  let lights = simple_area_lights;" << std::endl;
            return stream.str();
        }
    }

    // Write out basic information and start light table
    stream << "  let num_lights = " << mOrderedLights.size() << ";" << std::endl
           << "  let lights = @|id:i32| {" << std::endl;

    bool embedded = false;
    if (embedSimplePointLights) {
        stream << "    if id < " << mSimplePointLightCounter << " {" << std::endl
               << "      simple_point_lights(id)" << std::endl
               << "    }" << std::endl;

        embedded = true;
    }

    if (!skipArea && embedSimpleAreaLights) {
        if (embedSimplePointLights)
            stream << "    else if ";
        else
            stream << "    if ";

        stream << "id < " << mSimpleAreaLightCounter + mSimplePointLightCounter << " {" << std::endl
               << "      simple_area_lights(id - " << mSimplePointLightCounter << ")" << std::endl
               << "    }" << std::endl;

        embedded = true;
    }

    if (embedded)
        stream << "    else {" << std::endl;
    stream << "    match(id) {" << std::endl;

    counter = mSimplePointLightCounter + mSimpleAreaLightCounter;
    for (auto it = non_e_it; it != mOrderedLights.end(); ++it, ++counter) {
        const auto light = it->second;

        if (skip(light))
            continue;

        stream << "      " << counter << " => light_" << tree.getClosureID(it->first) << "," << std::endl;
    }

    stream << "      _ => make_null_light(id)" << std::endl;

    if (embedded)
        stream << "    }" << std::endl;

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}

void LoaderLight::prepare(LoaderContext& ctx)
{
    IG_LOG(L_DEBUG) << "Prepare lights" << std::endl;
    sortLights(ctx);

    // Setup area light registration
    setupAreaLights();
}

void LoaderLight::sortLights(LoaderContext& ctx)
{
    const auto& lights = ctx.Scene.lights();
    mOrderedLights.reserve(lights.size());
    for (const auto& p : lights)
        mOrderedLights.emplace_back(p);

    // Partition based on types
    auto spl_it = std::partition(mOrderedLights.begin(), mOrderedLights.end(), [](const auto& p) { return is_simple_point_light(p.second); });
    auto sal_it = std::partition(spl_it, mOrderedLights.end(), [](const auto& p) { return is_simple_area_light(p.second); });

    mSimplePointLightCounter = std::distance(mOrderedLights.begin(), spl_it);
    mSimpleAreaLightCounter  = std::distance(spl_it, sal_it);
}

void LoaderLight::setupAreaLights()
{
    for (size_t id = 0; id < mOrderedLights.size(); ++id) {
        const auto& light = mOrderedLights[id].second;
        if (light->pluginType() == "area") {
            const std::string entity = light->property("entity").getString();
            mAreaLights[entity]      = id;
        }
    }
}

void LoaderLight::embedLights(LoaderContext& ctx)
{
    constexpr size_t MaxSimpleInlineLights = 10;

    const size_t simpleLightCounter   = mSimplePointLightCounter + mSimpleAreaLightCounter;
    const bool embedSimpleLights      = simpleLightCounter > MaxSimpleInlineLights;
    const bool embedSimplePointLights = mSimplePointLightCounter > 0 && embedSimpleLights;
    const bool embedSimpleAreaLights  = mSimpleAreaLightCounter > 0 && embedSimpleLights;

    // Export simple point lights
    if (embedSimplePointLights)
        exportSimplePointLights(ctx);
    else
        mSimplePointLightCounter = 0;

    // Export simple area lights
    if (embedSimpleAreaLights)
        exportSimpleAreaLights(ctx);
    else
        mSimpleAreaLightCounter = 0;
}

std::filesystem::path LoaderLight::generateLightSelectionCDF(LoaderContext& ctx)
{
    const std::string exported_id = "_light_cdf_";

    const auto data = ctx.ExportedData.find(exported_id);
    if (data != ctx.ExportedData.end())
        return std::any_cast<std::string>(data->second);

    if (mOrderedLights.empty())
        return {}; // Fallback to null light selector

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/light_cdf.bin";

    std::vector<float> estimated_powers;
    estimated_powers.reserve(mOrderedLights.size());
    for (const auto& pair : mOrderedLights) {
        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == pair.second->pluginType()) {
                float power = _generators[i].Power(pair.second, ctx);
                estimated_powers.push_back(power);
                found = true;
                break;
            }
        }
        if (!found)
            estimated_powers.push_back(0);
    }

    CDF::computeForArray(estimated_powers, path);

    ctx.ExportedData[exported_id] = path;
    return path;
}
} // namespace IG