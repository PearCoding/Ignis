#pragma once

namespace IGGui {
bool BeginTimeline(const char* id);
void EndTimeline();

bool TimelineHeader(float min, float max, bool editable);

bool BeginTimelineRow();
void EndTimelineRow();

void TimelineEventUnformatted(float t_start, float t_end, const char* text);
void TimelineEvent(float t_start, float t_end, const char* fmt, ...);
} // namespace IGGui