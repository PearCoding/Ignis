#pragma once

#include "TechniqueInfo.h"

namespace IG {
class SceneObject;

constexpr int32 DefaultMaxRayDepth = 64; // This is quite large
constexpr int32 DefaultMinRayDepth = 2;  // The first bounce should not be affected by rr (by default)
class Technique {
public:
    inline Technique(const std::string& type)
        : mType(type)
    {
    }
    virtual ~Technique() = default;

    [[nodiscard]] inline const std::string& type() const { return mType; }

    [[nodiscard]] virtual TechniqueInfo getInfo(const LoaderContext& ctx) const = 0;

    struct SerializationInput {
        std::ostream& Stream;
        LoaderContext& Context;
    };
    virtual void generateBody(const SerializationInput& input) const = 0;

private:
    std::string mType;
};
} // namespace IG