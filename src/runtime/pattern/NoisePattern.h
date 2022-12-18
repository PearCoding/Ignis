#pragma once

#include "Pattern.h"

namespace IG {
class NoisePattern : public Pattern {
public:
    enum class Type {
        Noise,
        CellNoise,
        PNoise,
        Perlin,
        Voronoi,
        FBM
    };
    NoisePattern(Type type, const std::string& name, const std::shared_ptr<SceneObject>& obj);

    void serialize(const SerializationInput& input) const override;

private:
    std::shared_ptr<SceneObject> mObject;
    Type mType;
};
} // namespace IG