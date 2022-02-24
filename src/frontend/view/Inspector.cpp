#include "Inspector.h"
#include "IG_Config.h"

#include "imgui.h"

#include <Eigen/Core>
#include <array>
#include <numeric>
#include <tuple>

// ImGUI based image inspector inspired by
// https://github.com/CedricGuillemet/imgInspect

namespace IG {

static inline ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
    return ImVec2(a.x + b.x, a.y + b.y);
}

static inline ImVec2 operator*(const ImVec2& a, const ImVec2& b)
{
    return ImVec2(a.x * b.x, a.y * b.y);
}

static inline ImVec2 operator*(const ImVec2& a, float b)
{
    return ImVec2(a.x * b, a.y * b);
}

static inline uint32_t get_texel(int px, int py, size_t width, size_t height, const uint32_t* rgb)
{
    if (px < 0 || px >= (int)width || py < 0 || py >= (int)height)
        return IM_COL32(0, 0, 0, 0xFF);

    // We have a different storage then imgui expects
    uint32_t raw = rgb[py * width + px];
    return IM_COL32((raw & 0x00FF0000) >> 16, (raw & 0x0000FF00) >> 8, (raw & 0x000000FF), 0xFF);
}

static inline Eigen::Vector3f get_pixel(int px, int py, size_t width, size_t height, const float* values)
{
    if (px < 0 || px >= (int)width || py < 0 || py >= (int)height)
        return Eigen::Vector3f::Zero();
    else
        return Eigen::Vector3f(
            values[py * width * 3 + px * 3 + 0],
            values[py * width * 3 + px * 3 + 1],
            values[py * width * 3 + px * 3 + 2]);
}

void ui_inspect_image(int px, int py, size_t width, size_t height, float scale, const float* values, const uint32_t* rgb)
{
    if (px < 0 || px >= (int)width || py < 0 || py >= (int)height)
        return;

    IG_ASSERT(values != nullptr, "Expected valid framebuffer");
    IG_ASSERT(rgb != nullptr, "Expected valid rgb framebuffer");

    constexpr int ZoomSize             = 4;
    constexpr float ZoomRectangleWidth = 200;
    constexpr float QuadWidth          = ZoomRectangleWidth / (ZoomSize * 2 + 1);
    static std::array<float, (2 * ZoomSize + 1) * (2 * ZoomSize + 1)> lums;

    ImGui::BeginTooltip();
    ImGui::BeginGroup();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // bitmap zoom
    ImGui::InvisibleButton("_inspector_1", ImVec2(ZoomRectangleWidth, ZoomRectangleWidth));
    const ImVec2 rectMin = ImGui::GetItemRectMin();
    const ImVec2 rectMax = ImGui::GetItemRectMax();
    draw_list->AddRectFilled(rectMin, rectMax, 0xFF000000);

    const ImVec2 quadSize(QuadWidth, QuadWidth);

    const int basex = std::min(std::max(px, ZoomSize), (int)width - ZoomSize);
    const int basey = std::min(std::max(py, ZoomSize), (int)height - ZoomSize);
    for (int y = -ZoomSize; y <= ZoomSize; y++) {
        for (int x = -ZoomSize; x <= ZoomSize; x++) {
            const uint32_t texel = get_texel(x + basex, y + basey, width, height, rgb);
            ImVec2 pos           = rectMin + ImVec2(float(x + ZoomSize), float(y + ZoomSize)) * quadSize;
            draw_list->AddRectFilled(pos, pos + quadSize, texel);

            const Eigen::Vector3f pixel = get_pixel(x + basex, y + basey, width, height, values) * scale;

            int ind   = (y + ZoomSize) * (2 * ZoomSize + 1) + x + ZoomSize;
            lums[ind] = pixel.x() * 0.2126f + pixel.y() * 0.7152f + pixel.z() * 0.0722f; // Luminance
        }
    }
    ImGui::SameLine();

    std::sort(lums.begin(), lums.end());
    float mean = std::accumulate(lums.begin(), lums.end(), 0.0f) / (float)lums.size();
    float max  = std::reduce(lums.begin(), lums.end(), 0.0f, [](float a, float b) { return std::max(a, b); });
    float min  = std::reduce(lums.begin(), lums.end(), std::numeric_limits<float>::infinity(), [](float a, float b) { return std::min(a, b); });

    float median  = lums[lums.size() / 2];
    float softMax = lums[lums.size() - 2];
    float softMin = lums[1];

    // center quad
    const ImVec2 pos = rectMin + ImVec2(float(ZoomSize), float(ZoomSize)) * quadSize;
    draw_list->AddRect(pos, pos + quadSize, 0xFF0000FF, 0.f, 15, 2.f);

    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImVec4 color = ImColor(get_texel(px, py, width, height, rgb));
    ImVec4 colHSV;
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, colHSV.x, colHSV.y, colHSV.z);
    const Eigen::Vector3f pixel = get_pixel(px, py, width, height, values) * scale;
    ImGui::Text("Coord %d %d", px, py);
    ImGui::Separator();
    ImGui::Text("Raw");
    ImGui::Text("R %1.3f G %1.3f B %1.3f", pixel.x(), pixel.y(), pixel.z());
    ImGui::Text("Max  %1.3f, 95%%    %1.3f", max, softMax);
    ImGui::Text("Min  %1.3f,  5%%    %1.3f", min, softMin);
    ImGui::Text("Mean %1.3f, Median %1.3f", mean, median);
    ImGui::Separator();
    ImGui::Text("Mapped");
    ImGui::Text("R 0x%02x  G 0x%02x  B 0x%02x", int(color.x * 255.f), int(color.y * 255.f), int(color.z * 255.f));
    ImGui::Text("R %1.3f G %1.3f B %1.3f", color.x, color.y, color.z);
    ImGui::Text(
        "H 0x%02x  S 0x%02x  V 0x%02x", int(colHSV.x * 255.f), int(colHSV.y * 255.f), int(colHSV.z * 255.f));
    ImGui::Text("H %1.3f S %1.3f V %1.3f", colHSV.x, colHSV.y, colHSV.z);
    ImGui::EndGroup();
    ImGui::EndTooltip();
}
} // namespace IG
