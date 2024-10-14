#include "ColorbarGizmo.h"
#include "Application.h"
#include "Colormap.h"
#include "Logger.h"

#include "UI.h"

namespace IG {

constexpr int ColorbarWidth  = 16;
constexpr int ColorbarHeight = 128;

ColorbarGizmo::ColorbarGizmo()
    : mTexture(nullptr)
{
    setupTexture();
}

ColorbarGizmo::~ColorbarGizmo()
{
    if (mTexture)
        SDL_DestroyTexture((SDL_Texture*)mTexture);
}

void ColorbarGizmo::render(float min, float max)
{
    if (!mTexture)
        return;

    if (ImGui::BeginChild("#colormap", ImVec2(0, 0), false, ImGuiWindowFlags_NoInputs)) {
        const ImVec2 sz = ImGui::CalcTextSize("TEST");
        ImGui::Image(mTexture, ImVec2(ColorbarWidth, ColorbarHeight));
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, 0));
        ImGui::Text("%.2f lx", max);
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, ColorbarHeight - sz.y));
        ImGui::Text("%.2f lx", min);
    }
    ImGui::EndChild();
}

void ColorbarGizmo::setupTexture()
{
    auto texture = SDL_CreateTexture(Application::getRenderer(), SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, ColorbarWidth, ColorbarHeight);
    if (!texture) {
        IG_LOG(L_FATAL) << "Cannot create SDL texture: " << SDL_GetError() << std::endl;
        return;
    }

    uint8* pixels;
    int pitch = ColorbarWidth * 4;

    if (SDL_LockTexture(texture, nullptr, (void**)&pixels, &pitch) != 0) {
        IG_LOG(L_FATAL) << "Cannot lock SDL texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return;
    }

    for (int y = 0; y < ColorbarHeight; ++y) {
        const float t        = 1 - y / float(ColorbarHeight - 1);
        const Vector4f color = colormap::inferno(t);

        const uint8 r = static_cast<uint8>(std::clamp(color.x(), 0.0f, 1.0f) * 255);
        const uint8 g = static_cast<uint8>(std::clamp(color.y(), 0.0f, 1.0f) * 255);
        const uint8 b = static_cast<uint8>(std::clamp(color.z(), 0.0f, 1.0f) * 255);
        const uint8 a = 255;

        uint8* row = pixels + y * pitch;
        for (int x = 0; x < ColorbarWidth; ++x) {
            *(row++) = a;
            *(row++) = r;
            *(row++) = g;
            *(row++) = b;
        }
    }

    SDL_UnlockTexture(texture);

    mTexture = texture;
}
}; // namespace IG