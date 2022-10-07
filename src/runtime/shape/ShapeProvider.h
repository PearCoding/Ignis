#pragma once

#include "loader/LoaderContext.h"

namespace IG {

struct LoaderResult;

class ShapeProvider {
public:
    ShapeProvider()          = default;
    virtual ~ShapeProvider() = default;

    virtual std::string_view identifier() const                                                                        = 0;
    virtual size_t id() const                                                                                          = 0;
    virtual void handle(LoaderContext& ctx, LoaderResult& result, const std::string& name, const Parser::Object& elem) = 0;
    virtual std::string generateShapeCode(const LoaderContext& ctx)                                                    = 0;
    virtual std::string generateTraversalCode(const LoaderContext& ctx)                                                = 0;
};
} // namespace IG