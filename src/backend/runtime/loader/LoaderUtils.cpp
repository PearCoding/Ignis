#include "LoaderUtils.h"
#include "CDF.h"
#include "skysun/SunLocation.h"

#include <cctype>
#include <sstream>

namespace IG {
std::string LoaderUtils::inlineSceneInfo(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << "SceneInfo { num_entities = " << ctx.EntityCount << ", num_materials = " << ctx.Environment.Materials.size() << " }";
    return stream.str();
}

std::string LoaderUtils::inlineSceneBBox(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << "make_bbox(" << LoaderUtils::inlineVector(ctx.Environment.SceneBBox.min) << ", " << LoaderUtils::inlineVector(ctx.Environment.SceneBBox.max) << ")";
    return stream.str();
}

std::string LoaderUtils::escapeIdentifier(const std::string& name)
{
    IG_ASSERT(!name.empty(), "Given string should not be empty");

    std::string copy = name;
    if (!std::isalpha(copy[0])) {
        copy = "_" + copy;
    }

    for (size_t i = 1; i < copy.size(); ++i) {
        char& c = copy[i];
        if (!std::isalnum(c))
            c = '_';
    }

    return copy;
}

std::string LoaderUtils::inlineTransformAs2d(const Transformf& t)
{
    Matrix3f mat          = Matrix3f::Identity();
    mat.block<2, 2>(0, 0) = t.affine().block<2, 2>(0, 0);
    mat.block<2, 1>(0, 2) = t.translation().block<2, 1>(0, 0);
    return inlineMatrix(mat);
}

std::string LoaderUtils::inlineMatrix2d(const Matrix2f& mat)
{
    if (mat.isIdentity()) {
        return "mat2x2_identity()";
    } else {
        std::stringstream stream;
        stream << "make_mat2x2(" << inlineVector2d(mat.col(0)) << ", " << inlineVector2d(mat.col(1)) << ")";
        return stream.str();
    }
}

std::string LoaderUtils::inlineMatrix(const Matrix3f& mat)
{
    if (mat.isIdentity()) {
        return "mat3x3_identity()";
    } else {
        std::stringstream stream;
        stream << "make_mat3x3(" << inlineVector(mat.col(0)) << ", " << inlineVector(mat.col(1)) << ", " << inlineVector(mat.col(2)) << ")";
        return stream.str();
    }
}

std::string LoaderUtils::inlineMatrix34(const Eigen::Matrix<float, 3, 4>& mat)
{
    if (mat.isIdentity()) {
        return "mat3x4_identity()";
    } else {
        std::stringstream stream;
        stream << "make_mat3x4(" << inlineVector(mat.col(0)) << ", " << inlineVector(mat.col(1)) << ", " << inlineVector(mat.col(2)) << ", " << inlineVector(mat.col(3)) << ")";
        return stream.str();
    }
}

std::string LoaderUtils::inlineVector2d(const Vector2f& pos)
{
    std::stringstream stream;
    stream << "make_vec2(" << pos.x() << ", " << pos.y() << ")";
    return stream.str();
}

std::string LoaderUtils::inlineVector(const Vector3f& pos)
{
    std::stringstream stream;
    stream << "make_vec3(" << pos.x() << ", " << pos.y() << ", " << pos.z() << ")";
    return stream.str();
}

std::string LoaderUtils::inlineColor(const Vector3f& color)
{
    std::stringstream stream;
    stream << "make_color(" << color.x() << ", " << color.y() << ", " << color.z() << ", 1)";
    return stream.str();
}

ElevationAzimuth LoaderUtils::getEA(const Parser::Object& obj)
{
    if (obj.property("direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj.property("direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj.property("sun_direction").isValid()) {
        return ElevationAzimuth::fromDirection(obj.property("sun_direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj.property("elevation").isValid() || obj.property("azimuth").isValid()) {
        return ElevationAzimuth{ obj.property("elevation").getNumber(0), obj.property("azimuth").getNumber(0) };
    } else {
        TimePoint timepoint;
        MapLocation location;
        timepoint.Year     = obj.property("year").getInteger(timepoint.Year);
        timepoint.Month    = obj.property("month").getInteger(timepoint.Month);
        timepoint.Day      = obj.property("day").getInteger(timepoint.Day);
        timepoint.Hour     = obj.property("hour").getInteger(timepoint.Hour);
        timepoint.Minute   = obj.property("minute").getInteger(timepoint.Minute);
        timepoint.Seconds  = obj.property("seconds").getNumber(timepoint.Seconds);
        location.Latitude  = obj.property("latitude").getNumber(location.Latitude);
        location.Longitude = obj.property("longitude").getNumber(location.Longitude);
        location.Timezone  = obj.property("timezone").getNumber(location.Timezone);
        return computeSunEA(timepoint, location);
    }
}

Vector3f LoaderUtils::getDirection(const Parser::Object& obj)
{
    return getEA(obj).toDirection();
}

LoaderUtils::CDFData LoaderUtils::setup_cdf(LoaderContext& ctx, const std::string& filename)
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
} // namespace IG