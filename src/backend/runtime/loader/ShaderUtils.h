#pragma once

#include "LoaderContext.h"
#include "Target.h"

namespace IG {
class ShaderUtils {
public:
    static std::string constructDevice(Target target);
    static std::string generateDatabase();
    static std::string inlineSceneInfo(const LoaderContext& ctx);
    static std::string inlineSceneBBox(const LoaderContext& ctx);

    static std::string escapeIdentifier(const std::string& name);
    static std::string inlineTransformAs2d(const Transformf& t);
    static std::string inlineMatrix2d(const Matrix2f& mat);
    static std::string inlineMatrix(const Matrix3f& mat);
    static std::string inlineVector2d(const Vector2f& pos);
    static std::string inlineVector(const Vector3f& pos);
    static std::string inlineColor(const Vector3f& color);
};
} // namespace IG