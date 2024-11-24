#pragma once

#include "CameraOrientation.h"

namespace IG {
class SceneObject;
class LoaderContext;
class ShadingTree;

class Camera {
public:
    inline Camera(const std::string& type)
        : mType(type)
    {
    }
    virtual ~Camera() = default;

    [[nodiscard]] inline const std::string& type() const { return mType; }

    struct SerializationInput {
        std::ostream& Stream;
        ShadingTree& Tree;
    };
    virtual void serialize(const SerializationInput& input) const = 0;

    virtual CameraOrientation getOrientation(const LoaderContext& ctx) const = 0;

    struct FOV {
        bool Vertical;
        float Value; // Field of View in radians
    };
    static FOV extractFOV(SceneObject& obj);

private:
    std::string mType;
};
} // namespace IG