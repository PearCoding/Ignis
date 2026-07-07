#pragma once

#include "GLTexture.h"
#include "Widget.h"

#include <memory>

namespace IG {
class ColorbarGizmo {
public:
    ColorbarGizmo();
    virtual ~ColorbarGizmo();

    void render(float min, float max);

private:
    void setupTexture();
    std::unique_ptr<ui::GLTexture> mTexture;
};
} // namespace IG