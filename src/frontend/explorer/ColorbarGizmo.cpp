#include "ColorbarGizmo.h"
#include "Logger.h"

#include "UI.h"

namespace IG {
extern SDL_Renderer* sRenderer;

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

    constexpr float WhiteEfficiency = 179;

    if (ImGui::BeginChild("#colormap", ImVec2(0, 0), false, ImGuiWindowFlags_NoInputs)) {
        const ImVec2 sz = ImGui::CalcTextSize("TEST");
        ImGui::Image(mTexture, ImVec2(ColorbarWidth, ColorbarHeight));
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, 0));
        ImGui::Text("%.2f lx", max * WhiteEfficiency);
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, ColorbarHeight - sz.y));
        ImGui::Text("%.2f lx", min * WhiteEfficiency);
    }
    ImGui::EndChild();
}

static inline Vector4f poly6(const Vector4f& c0, const Vector4f& c1, const Vector4f& c2, const Vector4f& c3, const Vector4f& c4, const Vector4f& c5, const Vector4f& c6, float t)
{
    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

static inline Vector4f computeInferno(float t)
{
    static const Vector4f c0 = Vector4f(0.0002189403691192265f, 0.001651004631001012f, -0.01948089843709184f, 1);
    static const Vector4f c1 = Vector4f(0.1065134194856116f, 0.5639564367884091f, 3.932712388889277f, 1);
    static const Vector4f c2 = Vector4f(11.60249308247187f, -3.972853965665698f, -15.9423941062914f, 1);
    static const Vector4f c3 = Vector4f(-41.70399613139459f, 17.43639888205313f, 44.35414519872813f, 1);
    static const Vector4f c4 = Vector4f(77.162935699427f, -33.40235894210092f, -81.80730925738993f, 1);
    static const Vector4f c5 = Vector4f(-71.31942824499214f, 32.62606426397723f, 73.20951985803202f, 1);
    static const Vector4f c6 = Vector4f(25.13112622477341f, -12.24266895238567f, -23.07032500287172f, 1);

    return poly6(c0, c1, c2, c3, c4, c5, c6, t);
}

void ColorbarGizmo::setupTexture()
{
    auto texture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, ColorbarWidth, ColorbarHeight);
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
        const Vector4f color = computeInferno(t);

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