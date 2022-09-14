#pragma once

#include "loader/LoaderContext.h"

namespace IG {

struct LoaderResult;

class ShapeProvider {
public:
    virtual std::string_view identifier() const                         = 0;
    virtual void handle(LoaderContext& ctx, const Parser::Object& elem) = 0;
    virtual void setup(LoaderContext& ctx, LoaderResult& result)        = 0;
    virtual std::string generateTraversalCode(LoaderContext& ctx)       = 0;
};
} // namespace IG