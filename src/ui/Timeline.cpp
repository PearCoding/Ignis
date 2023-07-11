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

    sTimeline.Current = sTimeline.Data.GetOrAddByKey(id);
    PushOverrideID(id);

    curTimeline()->curRow = -1;

    return true;
}

void EndTimeline()
{
    ImGuiWindow* win = GetCurrentWindow();

    ImVec2 start(GetWindowContentRegionMin().x + win->Pos.x,
                 GetWindowContentRegionMax().y - GetTextLineHeightWithSpacing() + win->Pos.y);
    ImVec2 end = GetWindowContentRegionMax() + win->Pos;

    win->DrawList->AddRectFilled(start, end, ImColor(0, 0, 10), SliderRadius);

    PopID();

    sTimeline.Current = nullptr; // TODO
}

bool TimelineHeader(float min, float max, bool editable)
{
    if (!curTimeline()->initHeader) {
        curTimeline()->totalMin = min;
        curTimeline()->totalMax = max;

        curTimeline()->curMax     = curTimeline()->totalMax;
        curTimeline()->curMin     = curTimeline()->totalMin;
        curTimeline()->initHeader = true;
    }

    ImGui::Dummy(ImVec2(1, EventHeight));

    ImGui::PushID(-1);
    ImGui::PushItemWidth(ImGui::CalcItemWidth());
    bool res = RangeSliderFloat("", &curTimeline()->curMin, &curTimeline()->curMax, curTimeline()->totalMin, curTimeline()->totalMax, nullptr, editable ? 0 : ImGuiSliderFlags_ReadOnly);
    ImGui::PopItemWidth();
    ImGui::PopID();

    return res;
}

bool BeginTimelineRow()
{
    ++curTimeline()->curRow;
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

    const ImVec2 label_size = CalcTextSize(textBegin, textEnd, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));

    // Accept null ranges
    if (textBegin == textEnd)
        textBegin = textEnd = "";

    // Calculate length
    if (textEnd == nullptr)
        textEnd = textBegin + strlen(textBegin); // FIXME-OPT

    float avail_range = ts->curMax - ts->curMin;
    if (avail_range <= 0)
        return;

    float range = std::min(ts->curMax, t_end) - std::max(ts->curMin, t_start);
    if (range <= 0)
        return;

    // float norm_range = range / avail_range;

    float norm_start = (std::max(ts->curMin, t_start) - ts->curMin) / avail_range;
    float norm_end   = (std::min(ts->curMax, t_end) - ts->curMin) / avail_range;

    const float pos_start = ImLerp(frame_bb.Min.x, frame_bb.Max.x, norm_start);
    const float pos_end   = ImLerp(frame_bb.Min.x, frame_bb.Max.x, norm_end);

    auto rect = ImRect(pos_start, frame_bb.Min.y, pos_end, frame_bb.Max.y);
    if (rect.GetWidth() < 1) // At least 1 pixel wide
        rect.Max.x = rect.Min.x + 1;

    window->DrawList->AddRectFilled(rect.Min, rect.Max, GetCurrentEventColor(), style.GrabRounding);


    static const ImVec2 dot_size = CalcTextSize("...", NULL, true);
    if (textBegin != textEnd) {
        if (rect.GetWidth() < label_size.x + 2) {
            if (rect.GetWidth() > dot_size.x)
                RenderTextClipped(rect.Min, rect.Max, "...", NULL, NULL, ImVec2(0.5f, 0.5f));
        } else {
            if (g.LogEnabled)
                LogSetNextTextDecoration("{", "}");
            RenderTextClipped(rect.Min, rect.Max, textBegin, textEnd, NULL, ImVec2(0.5f, 0.5f));
        }
    }

    const ImRect tooltip_bb(rect.Min - ImVec2(2, 2), rect.Max + ImVec2(2, 2));
    if (IsMouseHoveringRect(tooltip_bb.Min, tooltip_bb.Max))
        SetTooltip("%s", textBegin);
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

} // namespace IGGui