#include "ParameterWidget.h"
#include "RenderWidget.h"

#include "imgui.h"

namespace IG {

ParameterWidget::ParameterWidget(RenderWidget* renderWidget)
    : Widget()
    , mRenderWidget(renderWidget)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2"
};

void ParameterWidget::onRender(Widget*)
{
    bool changed                        = false;
    RenderWidget::Parameters parameters = mRenderWidget->currentParameters();

    if (ImGui::Begin("Parameters")) {
        if (ImGui::BeginTabBar("#parameter_tab", 0)) {
            if (ImGui::BeginTabItem("View")) {
                if (ImGui::SliderFloat("Exposure", &parameters.ExposureFactor, -10.0f, 10.0f))
                    changed = true;
                if (ImGui::SliderFloat("Offset", &parameters.ExposureOffset, -10.0f, 10.0f))
                    changed = true;

                const char* current_method = ToneMappingMethodOptions[(int)parameters.ToneMappingMethod];
                if (ImGui::BeginCombo("Method", current_method)) {
                    for (int i = 0; i < IM_ARRAYSIZE(ToneMappingMethodOptions); ++i) {
                        bool is_selected = (current_method == ToneMappingMethodOptions[i]);
                        if (ImGui::Selectable(ToneMappingMethodOptions[i], is_selected)) {
                            if (parameters.ToneMappingMethod != (RenderWidget::ToneMappingMethod)i) {
                                parameters.ToneMappingMethod = (RenderWidget::ToneMappingMethod)i;
                                changed                      = true;
                            }
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (changed)
        mRenderWidget->updateParameters(parameters);
}

}; // namespace IG