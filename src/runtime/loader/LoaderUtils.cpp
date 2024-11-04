#include "LoaderUtils.h"
#include "CDF.h"
#include "LoaderEntity.h"
#include "Logger.h"

#include <cctype>
#include <sstream>

namespace IG {
std::string LoaderUtils::inlineSceneBBox(const LoaderContext& ctx)
{
#if 0
    std::stringstream stream;
    stream << "make_bbox(" << LoaderUtils::inlineVector(ctx.SceneBBox.min) << ", " << LoaderUtils::inlineVector(ctx.SceneBBox.max) << ")";
    return stream.str();
#else
    IG_UNUSED(ctx);
    return "scene_bbox"; // Defined in each respective shader
#endif
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

TimePoint LoaderUtils::getTimePoint(SceneObject& obj)
{
    TimePoint timepoint;
    timepoint.Year    = obj.property("year").getInteger(timepoint.Year);
    timepoint.Month   = obj.property("month").getInteger(timepoint.Month);
    timepoint.Day     = obj.property("day").getInteger(timepoint.Day);
    timepoint.Hour    = obj.property("hour").getInteger(timepoint.Hour);
    timepoint.Minute  = obj.property("minute").getInteger(timepoint.Minute);
    timepoint.Seconds = obj.property("seconds").getNumber(timepoint.Seconds);
    return timepoint;
}

MapLocation LoaderUtils::getLocation(SceneObject& obj)
{
    MapLocation location;
    location.Latitude  = obj.property("latitude").getNumber(location.Latitude);
    location.Longitude = obj.property("longitude").getNumber(location.Longitude);
    location.Timezone  = obj.property("timezone").getNumber(location.Timezone);
    return location;
}

ElevationAzimuth LoaderUtils::getEA(SceneObject& obj)
{
    if (obj.property("direction").isValid()) {
        return ElevationAzimuth::fromDirectionYUp(obj.property("direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj.property("sun_direction").isValid()) {
        return ElevationAzimuth::fromDirectionYUp(obj.property("sun_direction").getVector3(Vector3f(0, 0, 1)).normalized());
    } else if (obj.property("elevation").isValid() || obj.property("azimuth").isValid()) {
        return ElevationAzimuth{ obj.property("elevation").getNumber(0), obj.property("azimuth").getNumber(0) };
    } else {
        return computeSunEA(getTimePoint(obj), getLocation(obj));
    }
}

Vector3f LoaderUtils::getDirection(SceneObject& obj)
{
    return getEA(obj).toDirectionYUp();
}

LoaderUtils::CDF2DData LoaderUtils::setup_cdf2d(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate)
{
    std::string name = filename.stem().generic_string();
    Image image      = Image::load(filename);
    return setup_cdf2d(ctx, name, image, premultiplySin, compensate);
}

LoaderUtils::CDF2DData LoaderUtils::setup_cdf2d(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate)
{
    const std::string exported_id = "_cdf2d_" + name;
    const auto data               = ctx.Cache->ExportedData.find(exported_id);
    if (data != ctx.Cache->ExportedData.end())
        return std::any_cast<CDF2DData>(data->second);

    IG_LOG(L_DEBUG) << "Generating environment cdf for '" << name << "'" << std::endl;
    const Path path = ctx.CacheManager->directory() / ("cdf_" + LoaderUtils::escapeIdentifier(name) + ".bin");

    size_t slice_conditional = 0;
    size_t slice_marginal    = 0;
    CDF::computeForImage(image, path, slice_conditional, slice_marginal, premultiplySin, compensate);

    const CDF2DData cdf_data             = { path, slice_conditional, slice_marginal };
    ctx.Cache->ExportedData[exported_id] = cdf_data;
    return cdf_data;
}

LoaderUtils::CDF2DSATData LoaderUtils::setup_cdf2d_sat(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate)
{
    std::string name = filename.stem().generic_string();
    Image image      = Image::load(filename);
    return setup_cdf2d_sat(ctx, name, image, premultiplySin, compensate);
}

LoaderUtils::CDF2DSATData LoaderUtils::setup_cdf2d_sat(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate)
{
    const std::string exported_id = "_cdf2dsat_" + name;
    const auto data               = ctx.Cache->ExportedData.find(exported_id);
    if (data != ctx.Cache->ExportedData.end())
        return std::any_cast<CDF2DSATData>(data->second);

    IG_LOG(L_DEBUG) << "Generating environment cdf (SAT) for '" << name << "'" << std::endl;
    const Path path = ctx.CacheManager->directory() / ("cdf_" + LoaderUtils::escapeIdentifier(name) + ".bin");

    size_t size   = 0;
    size_t width  = 0;
    size_t height = 0;
    CDF::computeForImageSAT(image, path, size, width, height, premultiplySin, compensate);

    const CDF2DSATData cdf_data          = { path, size, width, height };
    ctx.Cache->ExportedData[exported_id] = cdf_data;
    return cdf_data;
}

LoaderUtils::CDF2DHierachicalData LoaderUtils::setup_cdf2d_hierachical(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate)
{
    std::string name = filename.stem().generic_string();
    Image image      = Image::load(filename);
    return setup_cdf2d_hierachical(ctx, name, image, premultiplySin, compensate);
}

LoaderUtils::CDF2DHierachicalData LoaderUtils::setup_cdf2d_hierachical(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate)
{
    const std::string exported_id = "_cdf2dhierachical_" + name;
    const auto data               = ctx.Cache->ExportedData.find(exported_id);
    if (data != ctx.Cache->ExportedData.end())
        return std::any_cast<CDF2DHierachicalData>(data->second);

    IG_LOG(L_DEBUG) << "Generating environment cdf (Hierachical) for '" << name << "'" << std::endl;
    const Path path = ctx.CacheManager->directory() / ("cdf_" + LoaderUtils::escapeIdentifier(name) + ".bin");

    size_t size   = 0;
    size_t slice  = 0;
    size_t levels = 0;
    CDF::computeForImageHierachical(image, path, size, slice, levels, premultiplySin, compensate);

#if 0
    size_t size_c = 0;
    for (size_t l = 0; l < levels; ++l) {
        size_c += image.width * image.height / (1 << (2 * l));

        const size_t p = (size_t)std::ceil(image.width * image.height * (4 - 1.0f / (1 << 2*l)) / 3);
        std::cout << size_c << "=" << p << std::endl;
    }
#endif

    const CDF2DHierachicalData cdf_data  = { path, size, slice, levels };
    ctx.Cache->ExportedData[exported_id] = cdf_data;
    return cdf_data;
}

} // namespace IG