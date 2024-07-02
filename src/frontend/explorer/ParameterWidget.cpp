#include "ParameterWidget.h"
#include "MenuItem.h"
#include "RenderWidget.h"
#include "Runtime.h"

#include "imgui.h"

namespace IG {

ParameterWidget::ParameterWidget(RenderWidget* renderWidget)
    : Widget()
    , mVisibleItem(nullptr)
    , mRenderWidget(renderWidget)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2", "AGX", "PbrNeutral"
};

void ParameterWidget::onRender(Widget*)
{
    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    constexpr int HeaderFlags = ImGuiTreeNodeFlags_DefaultOpen;

    bool changed                        = false;
    RenderWidget::Parameters parameters = mRenderWidget->currentParameters();

    Runtime* runtime = mRenderWidget->currentRuntime();
    if (!runtime)
        return;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Parameters", mVisibleItem ? &visibility : nullptr)) {
        if (ImGui::CollapsingHeader("View##parameters", HeaderFlags)) {
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
        }

        if (ImGui::CollapsingHeader("Glare##parameters", HeaderFlags)) {
            bool showOverlay = mRenderWidget->isOverlayVisible();
            if (ImGui::Checkbox("Overlay", &showOverlay))
                mRenderWidget->showOverlay(showOverlay);

            float multiplier = runtime->parameters().getFloat("_glare_multiplier");
            if (ImGui::SliderFloat("Multiplier", &multiplier, 0.001f, 10.0f))
                runtime->setParameter("_glare_multiplier", std::max(0.001f, multiplier));
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);

    if (changed)
        mRenderWidget->updateParameters(parameters);
}
}; // namespace IG