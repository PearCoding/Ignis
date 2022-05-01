#pragma once

#include "IG_Config.h"
#include <unordered_set>

namespace IG {
struct LoaderContext;
class Transpiler {
public:
    inline explicit Transpiler(const LoaderContext& ctx)
        : mContext(ctx)
    {
    }

    struct Result {
        std::string Expr;
        std::unordered_set<std::string> Textures;
        bool ScalarOutput; // Else it is a color
    };
    std::optional<Result> transpile(const std::string& expr, const std::string& uv_access, bool hasSurfaceInfo) const;

    inline const LoaderContext& context() const { return mContext; }

private:
    const LoaderContext& mContext;
};
} // namespace IG