#include "GLTexture.h"
#include "opengl/glad.h"

namespace IG::ui {
GLTexture::GLTexture()
{
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLTexture::~GLTexture()
{
    if (mTexture)
        glDeleteTextures(1, &mTexture);
}

void GLTexture::resize(int width, int height)
{
    if (width == mWidth && height == mHeight)
        return;

    mWidth  = width;
    mHeight = height;

    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::update(const uint32_t* pixels)
{
    if (mWidth <= 0 || mHeight <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, mTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    // The source is ARGB8888 packed as 0xAARRGGBB per uint32 (as produced for the
    // former SDL_PIXELFORMAT_ARGB8888 path); GL_BGRA + *_8_8_8_8_REV reads it directly.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}
} // namespace IG::ui
