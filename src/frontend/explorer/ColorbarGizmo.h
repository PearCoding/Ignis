#pragma once

#include "Widget.h"

namespace IG {
class ColorbarGizmo {
public:
    ColorbarGizmo();
    virtual ~ColorbarGizmo();

    void render(float min, float max);

private:
    void setupTexture();
    void* mTexture;
};
} // namespace IGmatrix