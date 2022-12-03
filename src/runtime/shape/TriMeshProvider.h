#pragma once

#include "ShapeProvider.h"
#include <mutex>

namespace IG {
class TriMeshProvider : public ShapeProvider {
public:
    TriMeshProvider()          = default;
    virtual ~TriMeshProvider() = default;

    inline std::string_view identifier() const override { return "trimesh"; }
    inline size_t id() const override { return 0; }

    void handle(LoaderContext& ctx, ShapeMTAccessor& acc, const std::string& name, const Parser::Object& elem) override;
    std::string generateShapeCode(const LoaderContext& ctx) override;
    std::string generateTraversalCode(const LoaderContext& ctx) override;

private:
    std::mutex mBvhMutex;
};
} // namespace IG