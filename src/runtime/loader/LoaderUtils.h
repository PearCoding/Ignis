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

    static TimePoint getTimePoint(const SceneObject& obj);
    static MapLocation getLocation(const SceneObject& obj);
    static ElevationAzimuth getEA(const SceneObject& obj);
    static Vector3f getDirection(const SceneObject& obj);

    using CDF2DData = std::tuple<std::string, size_t, size_t>;
    static CDF2DData setup_cdf2d(LoaderContext& ctx, const std::string& filename, bool premultiplySin, bool compensate = false);
    static CDF2DData setup_cdf2d(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate = false);

    using CDF2DSATData = std::tuple<std::string, size_t, size_t>;
    static CDF2DSATData setup_cdf2d_sat(LoaderContext& ctx, const std::string& filename, bool premultiplySin, bool compensate = false);
    static CDF2DSATData setup_cdf2d_sat(LoaderContext& ctx, const std::string& name, const Image& image, bool premultiplySin, bool compensate = false);
};
} // namespace IG