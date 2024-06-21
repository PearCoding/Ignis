#include "PropertyView.h"
#include "Runtime.h"
#include "StringUtils.h"

#include "imgui.h"

namespace IG::ui {

static inline void draw_header()
{
    constexpr int HeaderFlags = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoSort;
    ImGui::TableSetupColumn("Name", HeaderFlags | ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", HeaderFlags | ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
}

template <typename _Tp, typename _Hash, typename _Pred, typename _Alloc>
static inline size_t getPropertyCount(const std::unordered_map<std::string, _Tp, _Hash, _Pred, _Alloc>& map, bool hide_internal)
{
    if (hide_internal) {
        size_t counter = 0;
        for (auto& param : map) {
            if (!string_starts_with(param.first, "_"))
                ++counter;
        }
        return counter;
    } else {
        return map.size();
    }
}

bool ui_property_view(Runtime* runtime)
{
    constexpr int TableFlags  = ImGuiTableFlags_PadOuterX;
    constexpr int InputFlags  = ImGuiInputTextFlags_EnterReturnsTrue;
    constexpr int SliderFlags = ImGuiSliderFlags_None;
    constexpr int ColorFlags  = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;

    auto& registry = runtime->accessParameters();
    bool updated   = false;

    static bool hide_internal = true;
    ImGui::Checkbox("Hide Internal", &hide_internal);

    if (getPropertyCount(registry.IntParameters, hide_internal) != 0) {
        if (ImGui::CollapsingHeader("Integers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_integers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.IntParameters) {
                    const bool is_public = !string_starts_with(param.first, "_");

                    if (is_public || !hide_internal) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(param.first.c_str());
                        ImGui::TableNextColumn();

                        if (is_public) {
                            const std::string id = "##" + param.first;
                            if (ImGui::InputInt(id.c_str(), &param.second, 1, 100, InputFlags))
                                updated = true;
                        } else {
                            ImGui::Text("%i", param.second);
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (getPropertyCount(registry.FloatParameters, hide_internal) != 0) {
        if (ImGui::CollapsingHeader("Numbers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_numbers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.FloatParameters) {
                    const bool is_public = !string_starts_with(param.first, "_");

                    if (is_public || !hide_internal) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(param.first.c_str());
                        ImGui::TableNextColumn();

                        if (is_public) {
                            const float min      = std::min(0.0f, std::floor(param.second));
                            const float max      = std::max(1.0f, std::ceil(param.second));
                            const std::string id = "##" + param.first;
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::SliderFloat(id.c_str(), &param.second, min, max, "%.3f", SliderFlags))
                                updated = true;
                        } else {
                            ImGui::Text("%.3f", param.second);
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (getPropertyCount(registry.VectorParameters, hide_internal) != 0) {
        if (ImGui::CollapsingHeader("Vectors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_vectors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.VectorParameters) {
                    const bool is_public = !string_starts_with(param.first, "_");

                    if (is_public || !hide_internal) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(param.first.c_str());
                        ImGui::TableNextColumn();

                        if (is_public) {
                            const std::string id = "##" + param.first;
                            if (ImGui::InputFloat3(id.c_str(), param.second.data(), "%.3f", InputFlags))
                                updated = true;
                        } else {
                            ImGui::Text("[%.3f, %.3f, %.3f]", param.second.x(), param.second.y(), param.second.z());
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (getPropertyCount(registry.ColorParameters, hide_internal) != 0) {
        if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_colors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.ColorParameters) {
                    const bool is_public = !string_starts_with(param.first, "_");

                    if (is_public || !hide_internal) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(param.first.c_str());
                        ImGui::TableNextColumn();

                        if (ImGui::ColorEdit4(param.first.c_str(), param.second.data(), ColorFlags | (is_public ? ImGuiColorEditFlags_NoInputs : 0)))
                            updated = true;
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    return updated;
}
} // namespace IG
