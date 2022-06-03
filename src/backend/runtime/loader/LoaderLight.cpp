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

#include <chrono>

// TODO: Make use of the ShadingTree!!
namespace IG {

inline static Vector3f get_property_color(const Parser::Property& prop, const Vector3f& def)
{
    if (prop.canBeNumber()) {
        return Vector3f::Constant(prop.getNumber());
    } else {
        return prop.getVector3(def);
    }
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
        const Vector3f radiance = get_property_color(light->property("intensity"), Vector3f::Zero());

        auto& lightData = ctx.Database->CustomTables["SimplePoint"].addLookup(0, 0, 0); // We do not make use of the typeid
        VectorSerializer lightSerializer(lightData, false);
        lightSerializer.write(position);              // +3   = 3
        lightSerializer.write((uint32)0 /*Padding*/); // +1   = 4
        lightSerializer.write(radiance);              // +3   = 7
        lightSerializer.write((uint32)0 /*Padding*/); // +1   = 8
    }
}

static void light_point(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    auto pos = light->property("position").getVector3(); // FIXME: Has to be fixed (WHY?)
    tree.addColor("intensity", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_point_light(" << LoaderUtils::inlineVector(pos)
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
        const Vector3f radiance      = get_property_color(light->property("radiance"), Vector3f::Zero());

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

static void light_area(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    const std::string entityName = light->property("entity").getString();
    tree.addColor("radiance", *light, Vector3f::Constant(1.0f), true, ShadingTree::IM_Bare);

    Entity entity;
    if (!tree.context().Environment.EmissiveEntities.count(entityName)) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return;
    } else {
        entity = tree.context().Environment.EmissiveEntities.at(entityName);
    }

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

        stream << "  let ae_" << LoaderUtils::escapeIdentifier(name) << " = make_plane_area_emitter(" << LoaderUtils::inlineVector(origin)
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

        stream << "  let ae_" << LoaderUtils::escapeIdentifier(name) << " = make_shape_area_emitter(" << inline_entity(entity, shape_id)
               << ", device.load_specific_shape(" << shape.FaceCount << ", " << shape.VertexCount << ", " << shape.NormalCount << ", " << shape.TexCount << ", " << shape_offset << ", dtb.shapes));" << std::endl;
    }

    stream << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_area_light(ae_" << LoaderUtils::escapeIdentifier(name) << ", "
           << " @|tex_coords| { maybe_unused(tex_coords); " << tree.getInline("radiance") << " });" << std::endl;

    tree.endClosure();
}

static void light_directional(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();
    tree.addColor("irradiance", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_directional_light(" << LoaderUtils::inlineVector(dir)
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("irradiance") << ");" << std::endl;

    tree.endClosure();
}

static void light_spot(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();
    auto pos     = light->property("position").getVector3(); // FIXME: Has to be fixed (WHY?)

    tree.addColor("intensity", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addNumber("cutoff", *light, 30, true, ShadingTree::IM_Bare);
    tree.addNumber("falloff", *light, 20, true, ShadingTree::IM_Bare);

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_spot_light(" << LoaderUtils::inlineVector(pos)
           << ", " << LoaderUtils::inlineVector(dir)
           << ", rad(" << tree.getInline("cutoff") << ")"
           << ", rad(" << tree.getInline("falloff") << ")"
           << ", " << tree.getInline("intensity") << ");" << std::endl;

    tree.endClosure();
}

static void light_sun(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    tree.addNumber("sun_scale", *light, 1, true, ShadingTree::IM_Bare);
    tree.addNumber("sun_radius_scale", *light, 1, true, ShadingTree::IM_Bare);

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_sun_light(" << LoaderUtils::inlineVector(dir)
           << ", " << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("sun_radius_scale")
           << ", color_mulf(color_builtins::white, " << tree.getInline("sun_scale") << "));" << std::endl;

    tree.endClosure();
}

static void light_sky(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("scale", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);

    const std::string path = setup_sky(tree.context(), name, light);
    const auto cdf         = setup_cdf(tree.context(), path);
    const std::string id   = LoaderUtils::escapeIdentifier(name);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    stream << tree.pullHeader()
           << "  let sky_tex_" << id << " = make_image_texture(make_repeat_border(), make_bilinear_filter(), device.load_image(\"" << path << "\"), mat3x3_identity());" << std::endl // TODO: Refactor this out
           << "  let sky_cdf_" << id << " = cdf::make_cdf_2d_from_buffer(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
           << "  let light_" << id << "   = make_environment_light_textured(" << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("scale")
           << ", sky_tex_" << id << ", sky_cdf_" << id
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
}

static void light_cie_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    tree.addColor("zenith", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addColor("ground", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addNumber("ground_brightness", *light, 0.2f, true, ShadingTree::IM_Bare);

    const Matrix3f trans  = light->property("transform").getTransform().linear().transpose().inverse();
    const bool has_ground = light->property("has_ground").getBool(true);

    bool cloudy = (light->pluginType() == "cie_cloudy" || light->pluginType() == "ciecloudy");
    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_cie_sky_light(" << LoaderUtils::inlineSceneBBox(tree.context())
           << ", " << tree.getInline("zenith")
           << ", " << tree.getInline("ground")
           << ", " << tree.getInline("ground_brightness")
           << ", " << (cloudy ? "true" : "false")
           << ", " << (has_ground ? "true" : "false")
           << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;

    tree.endClosure();
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

static void light_cie_sunny_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    const bool clear = (light->pluginType() == "cie_clear" || light->pluginType() == "cieclear");

    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    if (ea.Elevation > 87 * Deg2Rad) {
        IG_LOG(L_WARNING) << " Sun too close to zenith, reducing elevation to 87 degrees" << std::endl;
        ea.Elevation = 87 * Deg2Rad;
    }

    const float turbidity = light->property("turbidity").getNumber(2.45f);

    constexpr float SkyIllum = 203;
    float zenithbrightness   = (1.376f * turbidity - 1.81) * std::tan(ea.Elevation) + 0.38f;
    if (!clear)
        zenithbrightness = (zenithbrightness + 8.6f * dir.y() + 0.123) / 2;
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

    tree.addColor("scale", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addColor("zenith", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addColor("ground", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
    tree.addNumber("ground_brightness", *light, 0.2f, true, ShadingTree::IM_Bare);

    const Matrix3f trans  = light->property("transform").getTransform().linear().transpose().inverse();
    const bool has_ground = light->property("has_ground").getBool(true);

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_cie_sunny_light(" << LoaderUtils::inlineSceneBBox(tree.context())
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

static void light_perez(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();

    auto ea      = extractEA(light);
    Vector3f dir = ea.toDirection();

    tree.addNumber("a", *light, 1.0f, true, ShadingTree::IM_Bare);
    tree.addNumber("b", *light, 1.0f, true, ShadingTree::IM_Bare);
    tree.addNumber("c", *light, 1.0f, true, ShadingTree::IM_Bare);
    tree.addNumber("d", *light, 1.0f, true, ShadingTree::IM_Bare);
    tree.addNumber("e", *light, 1.0f, true, ShadingTree::IM_Bare);

    const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

    bool usesLuminance = false;
    if (light->properties().count("luminance")) {
        tree.addColor("luminance", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
        usesLuminance = true;
    } else {
        tree.addColor("zenith", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
        usesLuminance = false;
    }

    stream << tree.pullHeader()
           << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_perez_light("
           << LoaderUtils::inlineSceneBBox(tree.context())
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

static void light_env(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("scale", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);

    const std::string id = LoaderUtils::escapeIdentifier(name);
    const bool useCDF    = light->property("cdf").getBool(true);

    // TODO: Make this work with PExpr and other custom textures!
    if (light->property("radiance").type() == Parser::PT_STRING) {
        const Matrix3f trans = light->property("transform").getTransform().linear().transpose().inverse();

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
                   << LoaderTexture::generate(tex_name, *tex, tree)
                   << "  let light_" << id << " = make_environment_light_textured_naive(" << LoaderUtils::inlineSceneBBox(tree.context())
                   << ", " << tree.getInline("scale")
                   << ", tex_" << LoaderUtils::escapeIdentifier(tex_name)
                   << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        } else {
            const auto cdf = setup_cdf(tree.context(), tex_path);
            stream << tree.pullHeader()
                   << LoaderTexture::generate(tex_name, *tex, tree)
                   << "  let cdf_" << id << "   = cdf::make_cdf_2d_from_buffer(device.load_buffer(\"" << std::get<0>(cdf) << "\"), " << std::get<1>(cdf) << ", " << std::get<2>(cdf) << ");" << std::endl
                   << "  let light_" << id << " = make_environment_light_textured(" << LoaderUtils::inlineSceneBBox(tree.context())
                   << ", " << tree.getInline("scale")
                   << ", tex_" << LoaderUtils::escapeIdentifier(tex_name)
                   << ", cdf_" << id
                   << ", " << LoaderUtils::inlineMatrix(trans) << ");" << std::endl;
        }
    } else {
        tree.addColor("radiance", *light, Vector3f::Ones(), true, ShadingTree::IM_Bare);
        stream << tree.pullHeader()
               << "  let light_" << id << " = make_environment_light(" << LoaderUtils::inlineSceneBBox(tree.context())
               << ", " << tree.getInline("scale")
               << ", " << tree.getInline("radiance") << ");" << std::endl;
    }

    tree.endClosure();
}

static void light_error(std::ostream& stream, const std::string& name)
{
    stream << "  let light_" << LoaderUtils::escapeIdentifier(name) << " = make_null_light() /* Error */;" << std::endl;
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
    { "spot", light_spot },
    { "sun", light_sun },
    { "sky", light_sky },
    { "cie_uniform", light_cie_env },
    { "cieuniform", light_cie_env },
    { "cie_cloudy", light_cie_env },
    { "ciecloudy", light_cie_env },
    { "cie_clear", light_cie_sunny_env },
    { "cieclear", light_cie_sunny_env },
    { "cie_intermediate", light_cie_sunny_env },
    { "cieintermediate", light_cie_sunny_env },
    { "perez", light_perez },
    { "uniform", light_env },
    { "env", light_env },
    { "envmap", light_env },
    { "constant", light_env },
    { "", nullptr }
};

// TODO: Refactor this function... its horrible
std::string LoaderLight::generate(ShadingTree& tree, bool skipArea)
{
    constexpr size_t MaxSimpleInlineLights = 10;

    const auto& lights = tree.context().Scene.lights();

    size_t simplePointLightCounter = 0;
    size_t simpleAreaLightCounter  = 0;
    if (lights.size() >= MaxSimpleInlineLights) {
        for (const auto& pair : lights) {
            const auto light = pair.second;

            if (is_simple_point_light(light))
                ++simplePointLightCounter;
            else if (!skipArea && is_simple_area_light(light))
                ++simpleAreaLightCounter;
        }
    }

    const size_t simpleLightCounter   = simplePointLightCounter + simpleAreaLightCounter;
    const bool embedSimpleLights      = simpleLightCounter > MaxSimpleInlineLights;
    const bool embedSimplePointLights = simplePointLightCounter > 0 && embedSimpleLights;
    const bool embedSimpleAreaLights  = simpleAreaLightCounter > 0 && embedSimpleLights;

    // Export simple point lights
    if (embedSimplePointLights)
        exportSimplePointLights(tree.context());
    else
        simplePointLightCounter = 0;

    // Export simple area lights
    if (embedSimpleAreaLights)
        exportSimpleAreaLights(tree.context());
    else
        simpleAreaLightCounter = 0;

    // This will be used for now
    auto skip = [&](const std::shared_ptr<Parser::Object>& light) { return skipArea && light->pluginType() == "area"; };

    std::stringstream stream;

    // Write all non-embedded lights to shader
    size_t counter = embedSimpleLights ? simpleLightCounter : 0;
    for (const auto& pair : lights) {
        const auto light = pair.second;

        if (skip(light)
            || (embedSimplePointLights && is_simple_point_light(light))
            || (embedSimpleAreaLights && is_simple_area_light(light)))
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
        if (!found) {
            IG_LOG(L_ERROR) << "No light type '" << light->pluginType() << "' available" << std::endl;
            light_error(stream, pair.first);
        }
    }

    // Add a new line for cosmetics if necessary :P
    if (counter != 0)
        stream << std::endl;

    // Load embedded point lights if necessary
    if (embedSimplePointLights) {
        stream << "  let simple_point_lights = load_simple_point_lights(device);" << std::endl;

        // Special case: Nothing except embedded simple point lights
        if (counter > 0 && counter == simplePointLightCounter) {
            stream << "  let num_lights = " << counter << ";" << std::endl
                   << "  let lights = simple_point_lights;" << std::endl;
            return stream.str();
        }
    }

    // Load embedded area lights if necessary
    if (embedSimpleAreaLights) {
        stream << "  let simple_area_lights = load_simple_area_lights(device, shapes);" << std::endl;

        // Special case: Nothing except embedded simple area lights
        if (counter > 0 && counter == simpleAreaLightCounter) {
            stream << "  let num_lights = " << counter << ";" << std::endl
                   << "  let lights = simple_area_lights;" << std::endl;
            return stream.str();
        }
    }

    // Write out basic information and start light table
    stream << "  let num_lights = " << counter << ";" << std::endl
           << "  let lights = @|id:i32| {" << std::endl;

    if (embedSimplePointLights) {
        stream << "    if id < " << simplePointLightCounter << " {" << std::endl
               << "      simple_point_lights(id)" << std::endl
               << "    }" << std::endl;
    }

    if (embedSimpleAreaLights) {
        if (embedSimplePointLights)
            stream << "    else if ";
        else
            stream << "    if ";

        stream << "id < " << simpleAreaLightCounter + simplePointLightCounter << " {" << std::endl
               << "      simple_area_lights(id - " << simplePointLightCounter << ")" << std::endl
               << "    }" << std::endl;
    }

    if (embedSimpleLights)
        stream << "    else {" << std::endl;
    stream << "    match(id) {" << std::endl;

    size_t counter2 = simplePointLightCounter + simpleAreaLightCounter;
    for (const auto& pair : lights) {
        const auto light = pair.second;

        if (skip(light))
            continue;

        const bool isSPL = embedSimplePointLights && is_simple_point_light(light);
        const bool isSAL = embedSimpleAreaLights && is_simple_area_light(light);
        if (!isSPL && !isSAL) {
            if (counter2 < counter - 1)
                stream << "      " << counter2;
            else
                stream << "      _";

            stream << " => light_" << LoaderUtils::escapeIdentifier(pair.first)
                   << "," << std::endl;
            ++counter2;
        }
    }

    if (counter == 0 || counter2 == 0) {
        if (!skipArea) // Don't trigger a warning if we skip areas
            IG_LOG(L_WARNING) << "Scene does not contain lights" << std::endl;
        stream << "      _ => make_null_light()" << std::endl;
    }

    if (embedSimpleLights)
        stream << "    }" << std::endl;

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