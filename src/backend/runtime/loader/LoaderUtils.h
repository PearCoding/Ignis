#pragma once

#include "LoaderContext.h"

namespace IG {
struct ElevationAzimuth;
struct Entity;

class LoaderUtils {
public:
    static std::string inlineSceneInfo(const LoaderContext& ctx);
    static std::string inlineSceneBBox(const LoaderContext& ctx);

    static std::string inlineEntity(const Entity& entity, uint32 shapeID);

    static std::string escapeIdentifier(const std::string& name);
    static std::string inlineTransformAs2d(const Transformf& t);
    static std::string inlineMatrix2d(const Matrix2f& mat);
    static std::string inlineMatrix(const Matrix3f& mat);
    static std::string inlineMatrix34(const Eigen::Matrix<float, 3, 4>& mat);
    static std::string inlineVector2d(const Vector2f& pos);
    static std::string inlineVector(const Vector3f& pos);
    static std::string inlineColor(const Vector3f& color);

    static ElevationAzimuth getEA(const Parser::Object& obj);
    static Vector3f getDirection(const Parser::Object& obj);

    using CDFData = std::tuple<std::string, size_t, size_t>;
    static CDFData setup_cdf(LoaderContext& ctx, const std::string& filename);
};
} // namespace IG