#pragma once

#include "ShapeProvider.h"
#include <mutex>

namespace IG {
class TriMeshProvider : public ShapeProvider {
public:
    inline std::string_view identifier() const override { return "trimesh"; }

    void handle(LoaderContext& ctx, LoaderResult& result, const std::string& name, const Parser::Object& elem) override;
    std::string generateTraversalCode(LoaderContext& ctx) override;

private:
    std::mutex mDtbMutex;
    std::mutex mBvhMutex;
};
} // namespace IG