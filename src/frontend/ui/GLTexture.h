#pragma once

#include "imgui.h"
#include <cstdint>

namespace IG::ui {
// A 2D OpenGL texture that mirrors a CPU pixel buffer for display through ImGui::Image.
class GLTexture {
public:
    GLTexture();
    ~GLTexture();

    GLTexture(const GLTexture&)            = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    // (Re)allocate the texture storage. Does nothing if the size is unchanged.
    void resize(int width, int height);

    // Upload a full-frame RGBA8 buffer (one uint32 per pixel, byte order R,G,B,A).
    void update(const uint32_t* pixels);

    inline ImTextureID id() const { return (ImTextureID)(intptr_t)mTexture; }
    inline int width() const { return mWidth; }
    inline int height() const { return mHeight; }

private:
    unsigned int mTexture = 0;
    int mWidth            = 0;
    int mHeight           = 0;
};
} // namespace IG::ui
