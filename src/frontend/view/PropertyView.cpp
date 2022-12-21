#include "PropertyView.h"
#include "Runtime.h"
#include "StringUtils.h"

#include "imgui.h"

namespace IG {

static inline void draw_header()
{
    constexpr int HeaderFlags = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoSort;
    ImGui::TableSetupColumn("Name", HeaderFlags | ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", HeaderFlags | ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
}

bool ui_property_view(Runtime* runtime)
{
    constexpr int TableFlags  = ImGuiTableFlags_PadOuterX;
    constexpr int InputFlags  = ImGuiInputTextFlags_EnterReturnsTrue;
    constexpr int SliderFlags = ImGuiSliderFlags_None;
    constexpr int ColorFlags  = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;

    auto& registry = runtime->accessParameters();
    bool updated   = false;

    if (!registry.IntParameters.empty()) {
        if (ImGui::CollapsingHeader("Integers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_integers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.IntParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
                        const std::string id = "##" + param.first;
                        if (ImGui::InputInt(id.c_str(), &param.second, 1, 100, InputFlags))
                            updated = true;
                    } else {
                        ImGui::Text("%i", param.second);
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (!registry.FloatParameters.empty()) {
        if (ImGui::CollapsingHeader("Numbers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_numbers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.FloatParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
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
                ImGui::EndTable();
            }
        }
    }

    if (!registry.VectorParameters.empty()) {
        if (ImGui::CollapsingHeader("Vectors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_vectors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.VectorParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
                        const std::string id = "##" + param.first;
                        if (ImGui::InputFloat3(id.c_str(), param.second.data(), "%.3f", InputFlags))
                            updated = true;
                    } else {
                        ImGui::Text("[%.3f, %.3f, %.3f]", param.second.x(), param.second.y(), param.second.z());
                    }
                }
                ImGui::EndTable();
            }
        }
    }

    if (!registry.ColorParameters.empty()) {
        if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_colors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.ColorParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (ImGui::ColorEdit4(param.first.c_str(), param.second.data(), ColorFlags | (changeable ? ImGuiColorEditFlags_NoInputs : 0)))
                        updated = true;
                }

                ImGui::EndTable();
            }
        }
    }

    return updated;
}
} // namespace IG
