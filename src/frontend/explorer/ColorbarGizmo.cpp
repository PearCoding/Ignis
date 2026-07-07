#include "ColorbarGizmo.h"
#include "Colormap.h"
#include "Logger.h"

#include "UI.h"

#include <algorithm>
#include <vector>

namespace IG {

constexpr int ColorbarWidth  = 16;
constexpr int ColorbarHeight = 128;

ColorbarGizmo::ColorbarGizmo()
{
    setupTexture();
}

ColorbarGizmo::~ColorbarGizmo() = default;

void ColorbarGizmo::render(float min, float max)
{
    if (!mTexture)
        return;

    if (ImGui::BeginChild("#colormap", ImVec2(0, 0), false, ImGuiWindowFlags_NoInputs)) {
        const ImVec2 sz = ImGui::CalcTextSize("TEST");
        ImGui::Image(mTexture->id(), ImVec2(ColorbarWidth, ColorbarHeight));
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, 0));
        ImGui::Text("%.2f lx", max);
        ImGui::SetCursorPos(ImVec2(ColorbarWidth + 5, ColorbarHeight - sz.y));
        ImGui::Text("%.2f lx", min);
    }
    ImGui::EndChild();
}

void ColorbarGizmo::setupTexture()
{
    mTexture = std::make_unique<ui::GLTexture>();
    mTexture->resize(ColorbarWidth, ColorbarHeight);

    std::vector<uint32_t> buffer((size_t)ColorbarWidth * ColorbarHeight);
    for (int y = 0; y < ColorbarHeight; ++y) {
        const float t        = 1 - y / float(ColorbarHeight - 1);
        const Vector4f color = colormap::inferno(t);

        const uint32_t r = static_cast<uint32_t>(std::clamp(color.x(), 0.0f, 1.0f) * 255);
        const uint32_t g = static_cast<uint32_t>(std::clamp(color.y(), 0.0f, 1.0f) * 255);
        const uint32_t b = static_cast<uint32_t>(std::clamp(color.z(), 0.0f, 1.0f) * 255);
        const uint32_t a = 255;

        // Packed as 0xAARRGGBB to match ui::GLTexture::update
        const uint32_t px = (a << 24) | (r << 16) | (g << 8) | b;
        for (int x = 0; x < ColorbarWidth; ++x)
            buffer[(size_t)y * ColorbarWidth + x] = px;
    }

    mTexture->update(buffer.data());
}
}; // namespace IG
