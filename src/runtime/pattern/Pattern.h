#pragma once

#include "SceneObjectProxy.h"

namespace IG {
class ShadingTree;
class SceneObject;

/// A pattern is synonym for a texture in the scene description
class Pattern : public SceneObjectProxy {
public:
    inline Pattern(const std::string& name, const std::string& type)
        : SceneObjectProxy(name, type)
    {
    }

    virtual ~Pattern() = default;

    struct SerializationInput {
        std::ostream& Stream;
        ShadingTree& Tree;
    };
    virtual void serialize(const SerializationInput& input) const = 0;
};
} // namespace IG