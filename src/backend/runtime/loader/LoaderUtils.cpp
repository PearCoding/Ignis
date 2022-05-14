#include "LoaderUtils.h"

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
} // namespace IG