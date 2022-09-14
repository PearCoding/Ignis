#pragma once

#include "loader/LoaderContext.h"

namespace IG {
class ShapeProvider {
public:
    virtual std::string_view identifier() const                   = 0;
    virtual std::string generateTraversalCode(LoaderContext& ctx) = 0;
};
} // namespace IG