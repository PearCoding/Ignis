#include "PolarSlider.h"

#include "imgui_internal.h"

using namespace ImGui;

namespace IG::ui {

static ImVec2 GetOffsetForAngle(float norm_angle, float radius)
{
    const float x = radius * std::sin(norm_angle);
    const float y = -radius * std::cos(norm_angle);
    return ImVec2(x, y);
}

enum class PolarSliderType {
    Full = 0,
    Half,
    Quarter
};

static PolarSliderType GetType(float v_min, float v_max)
{
    const float range = std::abs(v_max - v_min);
    if (range > Pi)
        return PolarSliderType::Full;
    else if (range > Pi2)
        return PolarSliderType::Half;
    else
        return PolarSliderType::Quarter;
}

struct PolarSliderBounds {
    ImVec2 CenterPos;
    ImVec2 TextPadding;
    float GrabRadius;
    float RadiusForGrabber;
    float RadiusForText;
};

static PolarSliderBounds ComputeBounds(const ImRect& frame_bb, bool hasText, float v_min, float v_max)
{
    const auto type           = GetType(v_min, v_max);
    const float grab_radius   = 4.0f; // FIXME: Should be part of style.
    const ImVec2 text_padding = hasText ? CalcTextSize("360°") : ImVec2(0, 0);

    ImVec2 center_p = frame_bb.GetCenter();
    ImVec2 max_size;
    if (type == PolarSliderType::Full) {
        max_size = ImVec2(frame_bb.GetWidth() - text_padding.x, frame_bb.GetHeight() - text_padding.y);
    } else if (type == PolarSliderType::Half) {
        // Cutout the left side
        center_p.x = frame_bb.Min.x + text_padding.x / 2;

        max_size = ImVec2(2 * frame_bb.GetWidth() - text_padding.x, frame_bb.GetHeight() - text_padding.y);
    } else {
        // Cutout the left and lower side
        center_p.x = frame_bb.Min.x + text_padding.x / 2;
        center_p.y = frame_bb.Max.y - text_padding.y / 2;

        max_size = ImVec2(2 * frame_bb.GetWidth() - text_padding.x, 2 * frame_bb.GetHeight() - text_padding.y);
    }

    const float radius_for_text    = std::max(0.0f, std::min(max_size.x / 2, max_size.y / 2));
    const float radius_for_grabber = std::max(0.0f, radius_for_text - 2 * grab_radius);

    return PolarSliderBounds{
        .CenterPos        = center_p,
        .TextPadding      = text_padding,
        .GrabRadius       = grab_radius,
        .RadiusForGrabber = radius_for_grabber,
        .RadiusForText    = radius_for_text
    };
}

static bool PolarSliderBehavior(const PolarSliderBounds& bounds, ImGuiID id, float* norm_angle, float v_min, float v_max)
{
    ImGuiContext& g = *GImGui;
    if ((g.LastItemData.ItemFlags & ImGuiItemFlags_ReadOnly))
        return false;

    const auto type = GetType(v_min, v_max);

    // Process clicking on the slider
    // TODO: Keyboard & Gamepad
    bool value_changed = false;
    if (g.ActiveId == id) {
        // Grab relative distance towards the grabs
        float angle = -1;
        for (int i = 0; i < 2; ++i) {
            if (g.ActiveIdSource == ImGuiInputSource_Mouse) {
                if (!g.IO.MouseDown[0]) {
                    ClearActiveID();
                } else {
                    ImVec2 dir = ImVec2(g.IO.MousePos.x - bounds.CenterPos.x, g.IO.MousePos.y - bounds.CenterPos.y);

                    if (type != PolarSliderType::Full)
                        dir.x = std::abs(dir.x);
                    if (type == PolarSliderType::Quarter)
                        dir.y = -std::abs(dir.y);

                    const float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);

                    angle = std::atan2(dir.x / dist, -dir.y / dist);
                    if (angle < 0)
                        angle += 2 * Pi;
                }
            }
        }
        // TODO: Keyboard & Gamepad

        if (angle >= 0) {
            // Apply result
            angle = angle;
            if (*norm_angle != angle) {
                *norm_angle   = angle;
                value_changed = true;
            }
        }
    }

    return value_changed;
}

bool PolarSliderFloat(const char* id_s, float* value, float v_min, float v_max, int increments)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (!window || window->SkipItems)
        return false;

    ImGuiContext& g         = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id        = window->GetID(id_s);
    const float w           = CalcItemWidth();

    const auto type    = GetType(v_min, v_max);
    const float aspect = type == PolarSliderType::Half ? 0.5f : 1.0f;

    const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + w * aspect, window->DC.CursorPos.y + w));

    ItemSize(frame_bb, style.FramePadding.y);
    if (!ItemAdd(frame_bb, id))
        return false;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(ImGuiMouseButton_Left, 0, id);

    const bool make_active = (clicked || g.NavActivateId == id);
    if (make_active && clicked)
        SetKeyOwner(ImGuiKey_MouseLeft, id);

    if (make_active) {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    // Calculate bounds
    const auto bounds = ComputeBounds(frame_bb, increments > 0, v_min, v_max);

    // Draw frame
    const ImU32 frame_col = GetColorU32(ImGuiCol_FrameBgActive);
    RenderNavCursor(frame_bb, id);
    // RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    const float abs_range     = std::abs(v_max - v_min);
    const bool is_full_circle = GetType(v_min, v_max) == PolarSliderType::Full;
    if (is_full_circle) {
        window->DrawList->AddCircle(bounds.CenterPos, bounds.RadiusForGrabber, frame_col);
    } else {
        window->DrawList->PathArcTo(bounds.CenterPos, bounds.RadiusForGrabber, -Pi2, abs_range - Pi2);
        window->DrawList->PathStroke(frame_col);
    }

    // Compute and handle grab
    float norm_angle         = (*value - v_min) / (v_max - v_min) * abs_range;
    const bool value_changed = PolarSliderBehavior(bounds, id, &norm_angle, v_min, v_max);
    if (value_changed) {
        *value = norm_angle / abs_range * (v_max - v_min) + v_min;
        MarkItemEdited(id);
    }

    // Draw text
    char value_buf[64];
    for (int i = 0; i < increments; ++i) {
        const float delta = 1.0f / (is_full_circle ? increments : (increments - 1));
        const float t     = i * delta;
        const float deg   = ImLerp(v_min, v_max, t) * Rad2Deg;

        const ImVec2 o0 = GetOffsetForAngle(i * delta * abs_range, bounds.RadiusForGrabber - 4);
        const ImVec2 o1 = GetOffsetForAngle(i * delta * abs_range, bounds.RadiusForGrabber + 8);

        window->DrawList->AddLine(
            ImVec2(bounds.CenterPos.x + o0.x, bounds.CenterPos.y + o0.y),
            ImVec2(bounds.CenterPos.x + o1.x, bounds.CenterPos.y + o1.y),
            frame_col);

        const char* value_buf_end = value_buf + ImFormatString(value_buf, 64, "%1.f°", deg);
        const ImVec2 op           = GetOffsetForAngle(i * delta * abs_range, bounds.RadiusForText);
        const ImVec2 text_size    = CalcTextSize(value_buf, value_buf_end);
        RenderText(ImVec2(bounds.CenterPos.x + op.x - text_size.x / 2, bounds.CenterPos.y + op.y - text_size.y / 2), value_buf, value_buf_end);
    }

    // Draw grabber
    const ImVec2 g0 = GetOffsetForAngle(norm_angle, bounds.RadiusForGrabber);
    window->DrawList->AddCircleFilled(ImVec2(bounds.CenterPos.x + g0.x, bounds.CenterPos.y + g0.y), bounds.GrabRadius, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab));

    return value_changed;
}
} // namespace IG::ui