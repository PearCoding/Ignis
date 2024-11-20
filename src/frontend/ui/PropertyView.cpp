#include "PropertyView.h"
#include "Runtime.h"
#include "StringUtils.h"

#include "UI.h"

namespace IG::ui {

static inline void draw_header()
{
    constexpr int HeaderFlags = ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoSort;
    ImGui::TableSetupColumn("Name", HeaderFlags | ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", HeaderFlags | ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
}

static int inputTextCallback(ImGuiInputTextCallbackData* data)
{
    std::string* str = (std::string*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        IM_ASSERT(data->Buf == str->c_str());
        str->resize(data->BufTextLen);
        data->Buf = (char*)str->c_str();
    }
    return 0;
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

bool ui_registry_view(Runtime* runtime, bool readonly)
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

                        if (!readonly && is_public) {
                            const std::string id = "##reg_int_" + param.first;
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

                        if (!readonly && is_public) {
                            const float min      = std::min(0.0f, std::floor(param.second));
                            const float max      = std::max(1.0f, std::ceil(param.second));
                            const std::string id = "##reg_num_" + param.first;
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

                        if (!readonly && is_public) {
                            const std::string id = "##reg_vec_" + param.first;
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

                        if (ImGui::ColorEdit4(param.first.c_str(), param.second.data(), ColorFlags | ((!readonly && is_public) ? ImGuiColorEditFlags_NoInputs : 0)))
                            updated = true;
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    if (getPropertyCount(registry.StringParameters, hide_internal) != 0) {
        if (ImGui::CollapsingHeader("Strings", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("_table_strings", 2, TableFlags)) {
                // Header
                draw_header();

                // Content
                for (auto& param : registry.StringParameters) {
                    const bool is_public = !string_starts_with(param.first, "_");

                    if (is_public || !hide_internal) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(param.first.c_str());
                        ImGui::TableNextColumn();

                        if (!readonly && is_public) {
                            const std::string id = "##reg_str_" + param.first;
                            if (ImGui::InputText(id.c_str(), (char*)param.second.c_str(), param.second.capacity() + 1, InputFlags, inputTextCallback, &param.second))
                                updated = true;
                        } else {
                            ImGui::TextUnformatted(param.second.c_str());
                        }
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    return updated;
}

bool ui_property_view(Runtime* runtime, bool readonly)
{
    if (runtime->sceneParameterDesc().empty(false))
        return false;

    constexpr int TableFlags  = ImGuiTableFlags_PadOuterX;
    constexpr int InputFlags  = ImGuiInputTextFlags_EnterReturnsTrue;
    constexpr int SliderFlags = ImGuiSliderFlags_None;
    constexpr int ColorFlags  = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;

    const auto& desc = runtime->sceneParameterDesc();
    auto& registry   = runtime->accessParameters();
    bool updated     = false;

    if (ImGui::BeginTable("_table_prop", 2, TableFlags)) {
        // Header
        draw_header();

        // Content (Int)
        for (auto& param : desc.intParameters()) {
            if (param.second.Internal)
                continue;

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(param.second.Display.c_str());
            ImGui::TableNextColumn();

            auto& value = registry.IntParameters[param.first];

            if (!readonly) {
                const std::string id = "##prop_int_" + param.first;

                if (param.second.Min == std::numeric_limits<int>::min() && param.second.Max == std::numeric_limits<int>::max()) {
                    if (ImGui::InputInt(id.c_str(), &value, param.second.Step, param.second.Step * 100, InputFlags))
                        updated = true;
                } else {
                    if (ImGui::SliderInt(id.c_str(), &value, param.second.Min, param.second.Max, "%d", SliderFlags))
                        updated = true;
                }
            } else {
                ImGui::Text("%i", value);
            }

            if (!param.second.Description.empty())
                ImGui::SetItemTooltip("%s", param.second.Description.c_str());
        }

        // Content (Float)
        for (auto& param : desc.floatParameters()) {
            if (param.second.Internal)
                continue;

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(param.second.Display.c_str());
            ImGui::TableNextColumn();

            auto& value = registry.FloatParameters[param.first];

            if (!readonly) {
                const std::string id = "##prop_num_" + param.first;
                ImGui::SetNextItemWidth(-1);
                if (std::isinf(param.second.Min) || std::isinf(param.second.Max)) {
                    if (ImGui::InputFloat(id.c_str(), &value, param.second.Step, param.second.Step * 100, "%.3f", InputFlags))
                        updated = true;
                } else {
                    if (ImGui::SliderFloat(id.c_str(), &value, param.second.Min, param.second.Max, "%.3f", SliderFlags))
                        updated = true;
                }
            } else {
                ImGui::Text("%.3f", value);
            }

            if (!param.second.Description.empty())
                ImGui::SetItemTooltip("%s", param.second.Description.c_str());
        }

        // Content (Vector)
        for (auto& param : desc.vectorParameters()) {
            if (param.second.Internal)
                continue;

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(param.second.Display.c_str());
            ImGui::TableNextColumn();

            auto& value = registry.VectorParameters[param.first];

            // TODO

            if (!readonly) {
                const std::string id = "##prop_vec_" + param.first;
                if (ImGui::InputFloat3(id.c_str(), value.data(), "%.3f", InputFlags))
                    updated = true;
            } else {
                ImGui::Text("[%.3f, %.3f, %.3f]", value.x(), value.y(), value.z());
            }

            if (!param.second.Description.empty())
                ImGui::SetItemTooltip("%s", param.second.Description.c_str());
        }

        // Content (Color)
        for (auto& param : desc.colorParameters()) {
            if (param.second.Internal)
                continue;

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(param.second.Display.c_str());
            ImGui::TableNextColumn();

            auto& value = registry.ColorParameters[param.first];

            if (ImGui::ColorEdit4(param.first.c_str(), value.data(), ColorFlags | (!readonly ? ImGuiColorEditFlags_NoInputs : 0)))
                updated = true;

            if (!param.second.Description.empty())
                ImGui::SetItemTooltip("%s", param.second.Description.c_str());
        }

        // Content (String)
        for (auto& param : desc.stringParameters()) {
            if (param.second.Internal)
                continue;

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(param.second.Display.c_str());
            ImGui::TableNextColumn();

            auto& value = registry.StringParameters[param.first];

            if (!readonly) {
                const std::string id = "##prop_str_" + param.first;
                if (ImGui::InputText(id.c_str(), (char*)value.c_str(), value.capacity() + 1, InputFlags, inputTextCallback, &value))
                    updated = true;
            } else {
                ImGui::TextUnformatted(value.c_str());
            }

            if (!param.second.Description.empty())
                ImGui::SetItemTooltip("%s", param.second.Description.c_str());
        }

        ImGui::EndTable();
    }

    return updated;
}
} // namespace IG::ui
