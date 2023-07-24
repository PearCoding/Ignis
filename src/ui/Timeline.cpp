#define IMGUI_DEFINE_MATH_OPERATORS

#include "Timeline.h"
#include "RangeSlider.h"

#include "imgui_internal.h"

#include <unordered_map>

using namespace ImGui;

namespace IGGui {
struct InstanceData {
    bool initHeader = false;
    float totalMin;
    float totalMax;
    float curMin;
    float curMax;
    int curRow = -1;

    ImGuiWindow* window = nullptr;
    ImVec2 tableMinPos;
    ImVec2 tableMaxPos;

    ImDrawListSplitter backgroundSplitter;

    InstanceData* previous;
};

struct GlobalData {
    ImPool<InstanceData> Data;
    InstanceData* Current;
};

static GlobalData sTimeline; // TODO: Better approach

constexpr float SliderRadius = 6;
constexpr float EventRadius  = 6;
constexpr float EventHeight  = 8;

inline InstanceData* curTimeline() { return sTimeline.Current; }

bool BeginTimeline(const char* label)
{
    ImGuiWindow* window = GetCurrentWindow();
    const ImGuiID id    = window->GetID(label);

    auto ts = sTimeline.Data.GetOrAddByKey(id);
    PushOverrideID(id);

    ts->curRow      = -1;
    ts->window      = window;
    ts->tableMinPos = ImVec2(10000, 10000);
    ts->tableMaxPos = ImVec2(0, 0);

    ts->backgroundSplitter.Split(window->DrawList, 2);
    ts->backgroundSplitter.SetCurrentChannel(window->DrawList, 1);

    ts->previous      = sTimeline.Current;
    sTimeline.Current = ts;

    return true;
}

void EndTimeline()
{
    PopID();

    ImGuiWindow* window   = GetCurrentWindow();
    const ImGuiContext& g = *GetCurrentContext();
    auto ts               = curTimeline();
    const auto table_bb   = ImRect(ts->tableMinPos, ts->tableMaxPos);
    if (ts->window == window && table_bb.GetArea() > 0) {
        if (IsMouseHoveringRect(table_bb.Min, table_bb.Max)) {
            const float mouse_abs_pos = g.IO.MousePos[0];

            window->DrawList->AddLine(ImVec2(mouse_abs_pos, table_bb.Min.y), ImVec2(mouse_abs_pos, table_bb.Max.y), ImColor(240, 0, 0));

            float pixel_norm_x = (mouse_abs_pos - table_bb.Min.x) / table_bb.GetWidth();
            float t            = ImLerp(ts->curMin, ts->curMax, pixel_norm_x);

            char value_buf[64];
            const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), "%.2f ms", t);

            ImVec2 start_pos        = ImVec2(mouse_abs_pos + 4, table_bb.Min.y + 4);
            const ImVec2 label_size = CalcTextSize(value_buf, value_buf_end, true);
            if (start_pos.x + label_size.x + 8 > table_bb.Max.x)
                start_pos.x -= label_size.x + 8;

            RenderFrame(start_pos - ImVec2(2, 2), start_pos + ImVec2(2, 2) + label_size, GetColorU32(ImGuiCol_FrameBg), true, g.Style.FrameRounding);
            PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(200, 0, 0));
            RenderTextClipped(start_pos, start_pos + label_size, value_buf, value_buf_end, &label_size);
            PopStyleColor();
        }
    }

    ts->backgroundSplitter.SetCurrentChannel(window->DrawList, 0);

    // Draw vertical lines for our viewing pleasure
    if (ts->window == window && table_bb.GetArea() > 0) {
        window->DrawList->AddRectFilled(table_bb.Min, table_bb.Max, ImColor(255, 255, 255, 40));

        const float trange = ts->curMax - ts->curMin;

        float dt = 1;
        if (trange <= 2)
            dt = 0.1f;
        else if (trange <= 20)
            dt = 1;
        else if (trange <= 200)
            dt = 10;
        else if (trange <= 2000)
            dt = 100;
        else
            dt = 1000;

        const auto frame_color = GetColorU32(ImGuiCol_TableBorderStrong);
        const float tmin       = std::ceil(ts->curMin / dt) * dt;
        const float tmax       = std::floor(ts->curMax / dt) * dt;
        for (float t = tmin; t <= tmax; t += dt) {
            if (t == ts->curMin) // Skip zero
                continue;

            float nt = (t - ts->curMin) / (trange);
            float x  = ImLerp(table_bb.Min.x, table_bb.Max.x, nt);

            window->DrawList->AddLine(ImVec2(x, table_bb.Min.y), ImVec2(x, table_bb.Max.y), frame_color);
        }
    }
    ts->backgroundSplitter.Merge(window->DrawList);

    sTimeline.Current = sTimeline.Current->previous;
}

bool TimelineHeader(float min, float max, const char* single_display_format, bool editable)
{
    if (!curTimeline()->initHeader) {
        curTimeline()->totalMin = min;
        curTimeline()->totalMax = max;

        curTimeline()->curMax     = curTimeline()->totalMax;
        curTimeline()->curMin     = curTimeline()->totalMin;
        curTimeline()->initHeader = true;
    }

    // Acquire some information about the x-axis
    ImGuiWindow* window          = GetCurrentWindow();
    curTimeline()->tableMinPos.x = ImMin(curTimeline()->tableMinPos.x, window->DC.CursorPos.x);
    curTimeline()->tableMaxPos.x = ImMax(curTimeline()->tableMaxPos.x, window->DC.CursorPos.x);

    ImGui::PushID(-1);
    ImGui::PushItemWidth(-1);
    bool res = RangeSliderFloat("", &curTimeline()->curMin, &curTimeline()->curMax, curTimeline()->totalMin, curTimeline()->totalMax, single_display_format, editable ? 0 : ImGuiSliderFlags_ReadOnly);
    ImGui::PopItemWidth();
    ImGui::PopID();

    curTimeline()->tableMinPos.x = ImMin(curTimeline()->tableMinPos.x, window->DC.CursorMaxPos.x);
    curTimeline()->tableMaxPos.x = ImMax(curTimeline()->tableMaxPos.x, window->DC.CursorMaxPos.x);
    return res;
}

bool BeginTimelineRow()
{
    ++curTimeline()->curRow;

    ImGuiWindow* window     = GetCurrentWindow();
    ImGuiContext& g         = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 font_size  = CalcTextSize("", NULL, true);

    // Ensure space is acquired
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, window->DC.CursorPos + ImVec2(0, font_size.y + style.FramePadding.y * 2.0f));

    // Ignore x, as this might be used in the context of tables. Keep y information as the row might be empty
    curTimeline()->tableMinPos.y = ImMin(curTimeline()->tableMinPos.y, window->DC.CursorPos.y);
    curTimeline()->tableMinPos.y = ImMin(curTimeline()->tableMinPos.y, window->DC.CursorMaxPos.y);
    curTimeline()->tableMaxPos.y = ImMax(curTimeline()->tableMaxPos.y, window->DC.CursorPos.y);
    curTimeline()->tableMaxPos.y = ImMax(curTimeline()->tableMaxPos.y, window->DC.CursorMaxPos.y);
    return true;
}

void EndTimelineRow()
{
}

static inline ImU32 GetCurrentEventColor()
{
    static ImU32 colors[] = {
        (ImU32)ImColor(0.450000f, 0.376630f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.450000f, 0.405978f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.450000f, 0.229891f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.112500f, 0.376630f, 1.0f),
        (ImU32)ImColor(0.435326f, 0.450000f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.141848f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.435326f, 0.112500f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.450000f, 0.141848f, 1.0f),
        (ImU32)ImColor(0.347283f, 0.450000f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.112500f, 0.200543f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.229891f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.288587f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.347283f, 0.112500f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.112500f, 0.288587f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.112500f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.450000f, 0.200543f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.171196f, 0.450000f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.450000f, 0.317935f, 1.0f),
        (ImU32)ImColor(0.259239f, 0.450000f, 0.112500f, 1.0f),
        (ImU32)ImColor(0.259239f, 0.112500f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.405978f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.171196f, 0.112500f, 0.450000f, 1.0f),
        (ImU32)ImColor(0.112500f, 0.317935f, 0.450000f, 1.0f)
    };

    return colors[curTimeline()->curRow % 23];
}

void TimelineEventEx(float t_start, float t_end, const char* textBegin, const char* textEnd)
{
    auto* ts = curTimeline();

    ImGuiWindow* window     = GetCurrentWindow();
    ImGuiContext& g         = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float w           = CalcItemWidth();

    const char* textDisplayEnd = FindRenderedTextEnd(textBegin, textEnd);

    const ImVec2 label_size = CalcTextSize(textBegin, textDisplayEnd, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));

    float avail_range = ts->curMax - ts->curMin;
    if (avail_range <= 0)
        return;

    float range = std::min(ts->curMax, t_end) - std::max(ts->curMin, t_start);
    if (range <= 0)
        return;

    float norm_start = (std::max(ts->curMin, t_start) - ts->curMin) / avail_range;
    float norm_end   = (std::min(ts->curMax, t_end) - ts->curMin) / avail_range;

    const float pos_start = ImLerp(frame_bb.Min.x, frame_bb.Max.x, norm_start);
    const float pos_end   = ImLerp(frame_bb.Min.x, frame_bb.Max.x, norm_end);

    auto rect = ImRect(pos_start, frame_bb.Min.y, pos_end, frame_bb.Max.y);
    if (rect.GetWidth() < 1) // At least 1 pixel wide
        rect.Max.x = rect.Min.x + 1;

    window->DrawList->AddRectFilled(rect.Min, rect.Max, GetCurrentEventColor(), style.GrabRounding);

    if (textBegin != textDisplayEnd) {
        if (rect.GetWidth() >= label_size.x)
            RenderTextClipped(rect.Min, rect.Max, textBegin, textDisplayEnd, NULL, ImVec2(0.5f, 0.5f));

        const ImRect tooltip_bb(rect.Min - ImVec2(2, 2), rect.Max + ImVec2(2, 2));
        const bool hovered = IsMouseHoveringRect(tooltip_bb.Min, tooltip_bb.Max);

        if (hovered && g.ActiveId == 0)
            SetTooltip("%s", textBegin);
        // SetTooltip("%.*s", (int)(textDisplayEnd - textBegin), textBegin);
    }

    curTimeline()->tableMinPos = ImMin(curTimeline()->tableMinPos, rect.Min);
    curTimeline()->tableMinPos = ImMin(curTimeline()->tableMinPos, rect.Max);
    curTimeline()->tableMaxPos = ImMax(curTimeline()->tableMaxPos, rect.Min);
    curTimeline()->tableMaxPos = ImMax(curTimeline()->tableMaxPos, rect.Max);
}

void TimelineEventV(float t_start, float t_end, const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const char *text, *text_end;
    ImFormatStringToTempBufferV(&text, &text_end, fmt, args);
    TimelineEventEx(t_start, t_end, text, text_end);
}

void TimelineEvent(float t_start, float t_end, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TimelineEventV(t_start, t_end, fmt, args);
    va_end(args);
}

void TimelineEventUnformatted(float t_start, float t_end, const char* text)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    TimelineEventEx(t_start, t_end, text, nullptr);
}

void TimelineBarrier(float time, unsigned int color)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const auto* ts = curTimeline();

    if (time >= ts->curMin && time <= ts->curMax) {
        const float trange  = ts->curMax - ts->curMin;
        const auto table_bb = ImRect(ts->tableMinPos, ts->tableMaxPos);
        const float nt      = (time - ts->curMin) / (trange);
        const float x       = ImLerp(table_bb.Min.x, table_bb.Max.x, nt);

        window->DrawList->AddLine(ImVec2(x, table_bb.Min.y), ImVec2(x, table_bb.Max.y), ImU32(color));
    }
}

} // namespace IGGui
