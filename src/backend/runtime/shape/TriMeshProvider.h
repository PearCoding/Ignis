#pragma once

#include "ShapeProvider.h"

namespace IG {
class TriMeshProvider : public ShapeProvider {
public:
    inline std::string_view identifier() const override { return "trimesh"; }

    void handle(LoaderContext& ctx, const Parser::Object& elem) override;
    void setup(LoaderContext& ctx, LoaderResult& result) override;
    std::string generateTraversalCode(LoaderContext& ctx) override;

private:
    std::vector<TriMesh> mMeshes;
};
} // namespace IG