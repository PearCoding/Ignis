#define IMGUI_DEFINE_MATH_OPERATORS

#include "RangeSlider.h"

#include <limits>

#include "imgui_internal.h"

// Based on: https://github.com/ocornut/imgui/issues/76
namespace ImGui {
extern template float ScaleRatioFromValueT<float, float, float>(ImGuiDataType data_type, float v, float v_min, float v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_size);
extern template float RoundScalarWithFormatT<float>(const char* format, ImGuiDataType data_type, float v);
} // namespace ImGui

using namespace ImGui;

namespace IG::ui {

static int sSlider2Selection = 0;
static bool RangeSliderBehavior(const ImRect& frame_bb, ImGuiID id, float* v1, float* v2, float v_min, float v_max, const char* format, ImGuiSliderFlags flags, ImRect* out_grab_bb1, ImRect* out_grab_bb2)
{
    // Those are the things we can do easily outside the SliderBehaviorT<> template, saves code generation.
    ImGuiContext& g = *GImGui;
    if ((g.LastItemData.InFlags & ImGuiItemFlags_ReadOnly) || (flags & ImGuiSliderFlags_ReadOnly))
        return false;

    const ImGuiStyle& style = g.Style;

    const ImGuiAxis axis      = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_logarithmic = (flags & ImGuiSliderFlags_Logarithmic) != 0;

    // Calculate bounds
    const float grab_padding          = 2.0f; // FIXME: Should be part of style.
    const float slider_sz             = (frame_bb.Max[axis] - frame_bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz                     = ImMin(style.GrabMinSize, slider_sz);
    const float slider_usable_sz      = slider_sz - grab_sz;
    const float slider_usable_pos_min = frame_bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = frame_bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    // Logarithmic stuff
    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    float zero_deadzone_halfsize   = 0.0f; // Only valid when is_logarithmic is true
    if (is_logarithmic) {
        // When using logarithmic sliders, we need to clamp to avoid hitting zero, but our choice of clamp value greatly affects slider precision. We attempt to use the specified precision to estimate a good lower bound.
        const int decimal_precision = ImParseFormatPrecision(format, 3);
        logarithmic_zero_epsilon    = ImPow(0.1f, (float)decimal_precision);
        zero_deadzone_halfsize      = (style.LogSliderDeadzone * 0.5f) / ImMax(slider_usable_sz, 1.0f);
    }

    // Process clicking on the slider
    // TODO: Keyboard & Gamepad
    float* vals[]      = { v1, v2 };
    bool value_changed = false;
    if (g.ActiveId == id) {
        // Grab relative distance towards the grabs
        bool new_value_available = false;
        float rel_grab_pos[]     = { 0.0f, 0.0f };
        for (int i = 0; i < 2; ++i) {
            if (g.ActiveIdSource == ImGuiInputSource_Mouse) {
                if (!g.IO.MouseDown[0]) {
                    ClearActiveID();
                } else {
                    const float mouse_abs_pos = g.IO.MousePos[axis];
                    if (g.ActiveIdIsJustActivated) {
                        float grab_t = ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, *vals[i], v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                        if (axis == ImGuiAxis_Y)
                            grab_t = 1.0f - grab_t;
                        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                        rel_grab_pos[i]      = fabsf(mouse_abs_pos - grab_pos);
                    }
                    new_value_available = true;
                }
            }
        }
        // TODO: Keyboard & Gamepad

        if (new_value_available) {
            if (g.ActiveIdIsJustActivated) {
                // Select the closest one
                sSlider2Selection = 0;
                float rel_min_pos = std::numeric_limits<float>::infinity();
                for (int i = 0; i < 2; ++i) {
                    if (rel_grab_pos[i] < rel_min_pos) {
                        rel_min_pos       = rel_grab_pos[i];
                        sSlider2Selection = i;
                    }
                }
            }

            // Apply standard
            bool set_new_value = false;
            float clicked_t    = 0.0f;
            if (g.ActiveIdSource == ImGuiInputSource_Mouse) {
                if (g.IO.MouseDown[0]) {
                    const float mouse_abs_pos = g.IO.MousePos[axis];
                    if (g.ActiveIdIsJustActivated) {
                        float grab_t = ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, *vals[sSlider2Selection], v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                        if (axis == ImGuiAxis_Y)
                            grab_t = 1.0f - grab_t;
                        const float grab_pos           = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
                        const bool clicked_around_grab = (mouse_abs_pos >= grab_pos - grab_sz * 0.5f - 1.0f) && (mouse_abs_pos <= grab_pos + grab_sz * 0.5f + 1.0f); // No harm being extra generous here.
                        g.SliderGrabClickOffset        = clicked_around_grab ? (mouse_abs_pos - grab_pos) : 0;
                    }
                    if (slider_usable_sz > 0.0f)
                        clicked_t = ImSaturate((mouse_abs_pos - g.SliderGrabClickOffset - slider_usable_pos_min) / slider_usable_sz);
                    if (axis == ImGuiAxis_Y)
                        clicked_t = 1.0f - clicked_t;
                    set_new_value = true;
                }
            }

            if (set_new_value) {
                float v_new = ScaleValueFromRatioT<float, float, float>(ImGuiDataType_Float, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                // Round to user desired precision based on format string
                if (!(flags & ImGuiSliderFlags_NoRoundToFormat))
                    v_new = RoundScalarWithFormatT<float>(format, ImGuiDataType_Float, v_new);

                // Apply result
                if (*vals[sSlider2Selection] != v_new) {
                    *vals[sSlider2Selection] = v_new;
                    value_changed            = true;
                }
            }
        }
    }

    if (*vals[0] > *vals[1])
        ImSwap(*vals[0], *vals[1]);

    if (slider_sz < 1.0f) {
        *out_grab_bb1 = ImRect(frame_bb.Min, frame_bb.Min);
        *out_grab_bb2 = ImRect(frame_bb.Min, frame_bb.Min);
    } else {
        ImRect* rects[] = { out_grab_bb1, out_grab_bb2 };
        for (int i = 0; i < 2; ++i) {
            // Output grab position so it can be displayed by the caller
            float grab_t = ScaleRatioFromValueT<float, float, float>(ImGuiDataType_Float, *vals[i], v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
            if (axis == ImGuiAxis_Y)
                grab_t = 1.0f - grab_t;
            const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
            if (axis == ImGuiAxis_X)
                *rects[i] = ImRect(grab_pos - grab_sz * 0.5f, frame_bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, frame_bb.Max.y - grab_padding);
            else
                *rects[i] = ImRect(frame_bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, frame_bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
        }
    }

    return value_changed;
}

// ~95% common code with ImGui::SliderFloat
bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max, const char* single_display_format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g         = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id        = window->GetID(label);
    const float w           = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, 0))
        return false;

    if (single_display_format == NULL)
        single_display_format = DataTypeGetInfo(ImGuiDataType_Float)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.InFlags);
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

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered
                                                                                            : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Actual slider behavior + render grab
    ImRect grab_bb1;
    ImRect grab_bb2;
    const bool value_changed = RangeSliderBehavior(frame_bb, id, v1, v2, v_min, v_max, single_display_format, flags, &grab_bb1, &grab_bb2);
    if (value_changed)
        MarkItemEdited(id);

    if (grab_bb1.Max.x < grab_bb2.Min.x)
        window->DrawList->AddRectFilled(ImVec2(grab_bb1.Max.x, frame_bb.Min.y), ImVec2(grab_bb2.Min.x, frame_bb.Max.y), GetColorU32(ImGuiCol_FrameBgActive));

    if (grab_bb1.Max.x > grab_bb1.Min.x)
        window->DrawList->AddRectFilled(grab_bb1.Min, grab_bb1.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);
    if (grab_bb2.Max.x > grab_bb2.Min.x)
        window->DrawList->AddRectFilled(grab_bb2.Min, grab_bb2.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[130];
    char* value_buf_end = value_buf + ImFormatString(value_buf, 64, single_display_format, *v1);
    value_buf_end[0]    = ' ';
    value_buf_end[1]    = '|';
    value_buf_end[2]    = ' ';
    value_buf_end += 3;
    value_buf_end += ImFormatString(value_buf_end, 64, single_display_format, *v2);

    if (g.LogEnabled)
        LogSetNextTextDecoration("{", "}");
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    return value_changed;
}

} // namespace IG::ui