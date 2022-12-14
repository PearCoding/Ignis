#pragma once

#include "SceneObjectProxy.h"

namespace IG {
class ShadingTree;
class VectorSerializer;
class SceneObject;

class Light : public SceneObjectProxy {
public:
    inline Light(const std::string& name, const std::string& type)
        : SceneObjectProxy(name, type)
        , mID(0)
    {
    }

    virtual ~Light() = default;

    virtual bool isInfinite() const { return false; }
    virtual bool isDelta() const { return false; }
    virtual std::optional<Vector3f> position() const { return std::nullopt; }
    virtual std::optional<Vector3f> direction() const { return std::nullopt; }
    virtual std::optional<std::string> entity() const { return std::nullopt; }

    virtual float computeFlux(const ShadingTree&) const { return 0; }

    struct SerializationInput {
        size_t ID;
        std::ostream& Stream;
        ShadingTree& Tree;
    };
    virtual void serialize(const SerializationInput& input) const = 0;

    virtual std::optional<std::string> getEmbedClass() const { return std::nullopt; }
    struct EmbedInput {
        VectorSerializer& Serializer;
        ShadingTree& Tree;
    };
    virtual void embed(const EmbedInput& input) const { IG_UNUSED(input); };

    inline size_t id() const { return mID; }
    inline void setID(size_t id) { mID = id; }

private:
    size_t mID;
};
} // namespace IG