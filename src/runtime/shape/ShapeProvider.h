#pragma once

#include "loader/LoaderContext.h"

#include <mutex>

namespace IG {

/// Simple class making sure the multithreaded loading process is synchronized
struct ShapeMTAccessor {
    std::mutex DatabaseAccessMutex;
};

class ShapeProvider {
public:
    ShapeProvider()          = default;
    virtual ~ShapeProvider() = default;

    virtual std::string_view identifier() const                                                               = 0;
    virtual size_t id() const                                                                                 = 0;
    virtual void handle(LoaderContext& ctx, ShapeMTAccessor& acc, const std::string& name, SceneObject& elem) = 0;
    virtual std::string generateShapeCode(const LoaderContext& ctx)                                           = 0;
    virtual std::string generateTraversalCode(const LoaderContext& ctx)                                       = 0;
};
} // namespace IG