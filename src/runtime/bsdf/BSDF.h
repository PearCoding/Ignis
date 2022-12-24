#pragma once

#include "SceneObjectProxy.h"

namespace IG {
class ShadingTree;
class SceneObject;

class BSDF : public SceneObjectProxy {
public:
    inline BSDF(const std::string& name, const std::string& type)
        : SceneObjectProxy(name, type)
    {
    }

    virtual ~BSDF() = default;

    struct SerializationInput {
        std::ostream& Stream;
        ShadingTree& Tree;
    };
    virtual void serialize(const SerializationInput& input) const = 0;

    static std::optional<float> getDielectricIOR(const std::string& name);

    struct ConductorSpec {
        Vector3f Eta;
        Vector3f Kappa;
    };
    static std::optional<ConductorSpec> getConductor(const std::string& name);

protected:
    static bool setupRoughness(const std::shared_ptr<SceneObject>& bsdf, const SerializationInput& input);
};
} // namespace IG