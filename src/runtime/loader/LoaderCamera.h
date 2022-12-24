#pragma once

#include "camera/CameraOrientation.h"

namespace IG {
class LoaderContext;
class LoaderCamera {
public:
    void setup(const LoaderContext& ctx);
    [[nodiscard]] std::string generate(LoaderContext& ctx) const;
    [[nodiscard]] CameraOrientation getOrientation(const LoaderContext& ctx) const;

    inline bool hasCamera() const { return mCamera != nullptr; }

    static std::vector<std::string> getAvailableTypes();

private:
    std::shared_ptr<class Camera> mCamera;
};
} // namespace IG