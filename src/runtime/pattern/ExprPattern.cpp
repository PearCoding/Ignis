#include "ExprPattern.h"
#include "Logger.h"
#include "SceneObject.h"
#include "StringUtils.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"
#include "loader/Transpiler.h"

namespace IG {
ExprPattern::ExprPattern(const std::string& name, const std::shared_ptr<SceneObject>& object)
    : Pattern(name, "expr")
    , mObject(object)
{
}

static inline void setupTranspiler(Transpiler& transpiler, const std::shared_ptr<SceneObject>& object, ShadingTree& tree)
{
    // Register available variables to transpiler as well
    for (const auto& pair : object->properties()) {
        if (string_starts_with(pair.first, "num_"))
            transpiler.registerCustomVariableNumber(pair.first.substr(4), "var_tex_" + tree.getClosureID(pair.first.substr(4)));
        else if (string_starts_with(pair.first, "color_"))
            transpiler.registerCustomVariableColor(pair.first.substr(6), "var_tex_" + tree.getClosureID(pair.first.substr(6)));
        else if (string_starts_with(pair.first, "vec_"))
            transpiler.registerCustomVariableVector(pair.first.substr(4), "var_tex_" + tree.getClosureID(pair.first.substr(4)));
        else if (string_starts_with(pair.first, "bool_"))
            transpiler.registerCustomVariableBool(pair.first.substr(5), "var_tex_" + tree.getClosureID(pair.first.substr(5)));
    }
}

void ExprPattern::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    std::string expr = mObject->property("expr").getString();
    if (expr.empty()) {
        IG_LOG(L_ERROR) << "Texture '" << name() << "' requires an expression" << std::endl;
        expr = "vec4(1, 0, 1, 1)"; // ~ Pink -> Mark as error
    }

    // Register on the shading tree first
    for (const auto& pair : mObject->properties()) {
        if (string_starts_with(pair.first, "num_"))
            input.Tree.addNumber(pair.first, *mObject, 0.0f);
        else if (string_starts_with(pair.first, "vec_"))
            input.Tree.addVector(pair.first, *mObject, Vector3f::Ones());
        else if (string_starts_with(pair.first, "color_"))
            input.Tree.addColor(pair.first, *mObject, Vector3f::Ones());
    }

    Transpiler transpiler(input.Tree);
    setupTranspiler(transpiler, mObject, input.Tree);

    // Transpile
    auto res    = transpiler.transpile(expr);
    bool failed = !res.has_value();
    if (failed) {
        // Mark as failed output
        res = Transpiler::Result{ "color_builtins::pink", {}, {}, false, false };
    }

    // Patch output to color
    std::string output;
    if (res.value().ScalarOutput)
        output = "make_gray_color(" + res.value().Expr + ")";
    else
        output = res.value().Expr;

    // Make sure all texture is loaded
    for (const auto& used_tex : res.value().Textures)
        input.Tree.registerTextureUsage(used_tex);

    // Pull texture usage
    const std::string tex_id = input.Tree.getClosureID(name());
    input.Stream << input.Tree.pullHeader()
                 << "  let tex_" << tex_id << " : Texture = @|ctx| -> Color {" << std::endl;

    if (!failed) {
        // Inline custom variables
        for (const auto& pair : mObject->properties()) {
            if (string_starts_with(pair.first, "num_")) {
                const std::string var_id = input.Tree.getClosureID(pair.first.substr(4));
                input.Stream << "    let var_tex_" << var_id << " = " << input.Tree.getInline(pair.first) << ";" << std::endl;
            } else if (string_starts_with(pair.first, "color_")) {
                const std::string var_id = input.Tree.getClosureID(pair.first.substr(6));
                input.Stream << "    let var_tex_" << var_id << " = " << input.Tree.getInline(pair.first) << ";" << std::endl;
            } else if (string_starts_with(pair.first, "vec_")) {
                const std::string var_id = input.Tree.getClosureID(pair.first.substr(4));
                input.Stream << "    let var_tex_" << var_id << " = " << input.Tree.getInline(pair.first) << ";" << std::endl;
            } else if (string_starts_with(pair.first, "bool_")) {
                const std::string var_id = input.Tree.getClosureID(pair.first.substr(5));
                input.Stream << "    let var_tex_" << var_id << " = " << (pair.second.getBool() ? "true" : "false") << ";" << std::endl;
            }
        }
    }

    // End output
    input.Stream << "    " << output << " };" << std::endl;

    input.Tree.endClosure();
}

std::pair<size_t, size_t> ExprPattern::computeResolution(ShadingTree& tree) const
{
    std::string expr = mObject->property("expr").getString();
    if (expr.empty())
        return { 1, 1 };

    Transpiler transpiler(tree);
    setupTranspiler(transpiler, mObject, tree);
    auto res = transpiler.transpile(expr);
    if (!res.has_value())
        return { 1, 1 };

    std::pair<size_t, size_t> resolution = tree.computeTextureResolution(name(), res.value());
    for (const auto& pair : mObject->properties()) {
        if (string_starts_with(pair.first, "num_") || string_starts_with(pair.first, "color_")) {
            std::pair<size_t, size_t> param_res = tree.computeTextureResolution(pair.first, *mObject);
            resolution.first                    = std::max(resolution.first, param_res.first);
            resolution.second                   = std::max(resolution.second, param_res.second);
        }
    }

    return resolution;
}
} // namespace IG