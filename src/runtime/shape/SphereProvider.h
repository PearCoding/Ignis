#pragma once

#include "ShapeProvider.h"

namespace IG {
class SphereProvider : public ShapeProvider {
public:
    inline std::string_view identifier() const override { return "sphere"; }
};
} // namespace IG