#pragma once

#include "camera/CameraOrientation.h"

namespace IG {
class LoaderContext;
class ShadingTree;
class LoaderCamera {
public:
    void setup(const LoaderContext& ctx);
    [[nodiscard]] std::string generate(ShadingTree& tree) const;
    [[nodiscard]] CameraOrientation getOrientation(const LoaderContext& ctx) const;

    inline bool hasCamera() const { return mCamera != nullptr; }

    static std::vector<std::string> getAvailableTypes();

private:
    std::shared_ptr<class Camera> mCamera;
};
} // namespace IG