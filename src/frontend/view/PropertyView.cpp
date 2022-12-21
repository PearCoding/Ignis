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

static std::unordered_map<std::string, int> sIntParameters;
static std::unordered_map<std::string, float> sNumParameters;
static AlignedUnorderedMap<std::string, Vector3f> sVecParameters;
static AlignedUnorderedMap<std::string, Vector4f> sColParameters;

bool ui_property_view(Runtime* runtime)
{
    constexpr int TableFlags  = ImGuiTableFlags_PadOuterX;
    constexpr int InputFlags  = ImGuiInputTextFlags_EnterReturnsTrue;
    constexpr int SliderFlags = ImGuiSliderFlags_None;
    constexpr int ColorFlags  = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;

    const auto& intParams = runtime->getIntParameters();
    const auto& numParams = runtime->getNumberParameters();
    const auto& vecParams = runtime->getVectorParameters();
    const auto& colParams = runtime->getColorParameters();

    // Initialize internal storage if necessary
    if (sIntParameters.size() != intParams.size()
        || sNumParameters.size() != numParams.size()
        || sVecParameters.size() != vecParams.size()
        || sColParameters.size() != colParams.size()) {
        sIntParameters = intParams;
        sNumParameters = numParams;
        sVecParameters = vecParams;
        sColParameters = colParams;
    }

    bool updated = false;

    if (!intParams.empty()) {
        if (ImGui::CollapsingHeader("Integers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_integers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : sIntParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
                        const std::string id = "##" + param.first;
                        if (ImGui::InputInt(id.c_str(), &param.second, 1, 100, InputFlags)) {
                            runtime->setParameter(param.first, param.second);
                            updated = true;
                        }
                    } else {
                        ImGui::Text("%i", param.second);
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    if (!numParams.empty()) {
        if (ImGui::CollapsingHeader("Numbers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_numbers", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : sNumParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
                        const float min      = std::min(0.0f, std::floor(param.second));
                        const float max      = std::max(1.0f, std::ceil(param.second));
                        const std::string id = "##" + param.first;
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::SliderFloat(id.c_str(), &param.second, min, max, "%.3f", SliderFlags)) {
                            runtime->setParameter(param.first, param.second);
                            updated = true;
                        }
                    } else {
                        ImGui::Text("%.3f", param.second);
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    if (!vecParams.empty()) {
        if (ImGui::CollapsingHeader("Vectors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_vectors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : sVecParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (changeable) {
                        const std::string id = "##" + param.first;
                        if (ImGui::InputFloat3(id.c_str(), param.second.data(), "%.3f", InputFlags)) {
                            runtime->setParameter(param.first, param.second);
                            updated = true;
                        }
                    } else {
                        ImGui::Text("[%.3f, %.3f, %.3f]", param.second.x(), param.second.y(), param.second.z());
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    if (!colParams.empty()) {
        if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_colors", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : sColParameters) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::TableNextColumn();
                    const bool changeable = !string_starts_with(param.first, "_");

                    if (ImGui::ColorEdit4(param.first.c_str(), param.second.data(), ColorFlags | (changeable ? ImGuiColorEditFlags_NoInputs : 0))) {
                        runtime->setParameter(param.first, param.second);
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
