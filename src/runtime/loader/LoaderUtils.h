#pragma once

#include "Image.h"
#include "LoaderContext.h"
#include "skysun/SunLocation.h"

namespace IG {
struct ElevationAzimuth;
struct Entity;

class LoaderUtils {
public:
    static std::string inlineSceneBBox(const LoaderContext& ctx);
    static std::string inlineEntity(const Entity& entity);

    static std::string escapeIdentifier(const std::string& name);
    static std::string inlineTransformAs2d(const Transformf& t);
    static std::string inlineMatrix2d(const Matrix2f& mat);
    static std::string inlineMatrix(const Matrix3f& mat);
    static std::string inlineMatrix34(const Matrix34f& mat);
    static std::string inlineVector2d(const Vector2f& pos);
    static std::string inlineVector(const Vector3f& pos);
    static std::string inlineColor(const Vector3f& color);

    static TimePoint getTimePoint(SceneObject& obj);
    static MapLocation getLocation(SceneObject& obj);
    static ElevationAzimuth getEA(SceneObject& obj);
    static Vector3f getDirection(SceneObject& obj);

    using CDF2DData = std::tuple<Path, size_t, size_t>;
    static CDF2DData setup_cdf2d(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate = false);
    static CDF2DData setup_cdf2d(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate = false);

    using CDF2DSATData = std::tuple<Path, size_t, size_t, size_t>;
    static CDF2DSATData setup_cdf2d_sat(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate = false);
    static CDF2DSATData setup_cdf2d_sat(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate = false);

    using CDF2DHierachicalData = std::tuple<Path, size_t, size_t, size_t>;
    static CDF2DHierachicalData setup_cdf2d_hierachical(LoaderContext& ctx, const Path& filename, bool premultiplySin, bool compensate = false);
    static CDF2DHierachicalData setup_cdf2d_hierachical(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate = false);
};
} // namespace IG