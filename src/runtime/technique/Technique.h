#pragma once

#include "TechniqueInfo.h"

namespace IG {
namespace Parser {
class Object;
}

constexpr int32 DefaultMaxRayDepth = 64;
class Technique {
public:
    inline Technique(const std::string& type)
        : mType(type)
    {
    }
    virtual ~Technique() = default;

    [[nodiscard]] inline const std::string& type() const { return mType; }

    [[nodiscard]] inline virtual bool hasDenoiserSupport() const { return false; }
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