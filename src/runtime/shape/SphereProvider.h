#pragma once

#include "ShapeProvider.h"

namespace IG {
class SphereProvider : public ShapeProvider {
public:
    SphereProvider()          = default;
    virtual ~SphereProvider() = default;

    inline std::string_view identifier() const override { return "sphere"; }
    inline size_t id() const override { return 1; }

    void handle(LoaderContext& ctx, ShapeMTAccessor& acc, const std::string& name, const SceneObject& elem) override;
    std::string generateShapeCode(const LoaderContext& ctx) override;
    std::string generateTraversalCode(const LoaderContext& ctx) override;
};
} // namespace IG