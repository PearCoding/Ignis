#pragma once

#include "CameraOrientation.h"

namespace IG {
namespace Parser {
class Object;
}

class LoaderContext;
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
        const LoaderContext& Context;
    };
    virtual void serialize(const SerializationInput& input) const = 0;

    virtual CameraOrientation getOrientation(const LoaderContext& ctx) const = 0;

    struct FOV {
        bool Vertical;
        float Value; // Field of View in radians
    };
    static FOV extractFOV(const Parser::Object& obj);

private:
    std::string mType;
};
} // namespace IG