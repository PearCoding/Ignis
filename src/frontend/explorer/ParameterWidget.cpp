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
    , mKeepSizeSynced(true)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2", "AGX", "PbrNeutral"
};

static const char* const OverlayMethodOptions[] = {
    "None", "Luminance", "Luminance Squared", "Glare Source"
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

    const bool hasTonemapping = parameters.OverlayMethod == RenderWidget::OverlayMethod::None || parameters.OverlayMethod == RenderWidget::OverlayMethod::GlareSource;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Parameters", mVisibleItem ? &visibility : nullptr)) {
        if (ImGui::CollapsingHeader("View##parameters", HeaderFlags)) {
            const char* current_overlay = OverlayMethodOptions[(int)parameters.OverlayMethod];
            if (ImGui::BeginCombo("Overlay", current_overlay)) {
                for (int i = 0; i < IM_ARRAYSIZE(OverlayMethodOptions); ++i) {
                    bool is_selected = (current_overlay == OverlayMethodOptions[i]);
                    if (ImGui::Selectable(OverlayMethodOptions[i], is_selected)) {
                        if (parameters.OverlayMethod != (RenderWidget::OverlayMethod)i) {
                            parameters.OverlayMethod = (RenderWidget::OverlayMethod)i;
                            changed                  = true;
                        }
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (!hasTonemapping)
                ImGui::BeginDisabled();

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

            if (!hasTonemapping)
                ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Glare##parameters", 0)) {
            float multiplier = runtime->parameters().getFloat("_glare_multiplier");
            if (ImGui::SliderFloat("Multiplier", &multiplier, 0.001f, 10.0f))
                runtime->setParameter("_glare_multiplier", std::max(0.001f, multiplier));
        }

        if (ImGui::CollapsingHeader("Renderer##parameters", 0)) {
            const auto internalSize = mRenderWidget->internalViewSize();

            ImGui::Checkbox("Keep equal", &mKeepSizeSynced);

            int width = (int)internalSize.first;
            ImGui::SliderInt("Width", &width, 32, 4096);

            if (mKeepSizeSynced)
                ImGui::BeginDisabled();
            int height = mKeepSizeSynced ? width : (int)internalSize.second;
            ImGui::SliderInt("Height", &height, 32, 4096);
            if (mKeepSizeSynced)
                ImGui::EndDisabled();

            if (width != (int)internalSize.first || height != (int)internalSize.second)
                mRenderWidget->resizeInternalView((size_t)std::max(32, width), (size_t)std::max(32, height));
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);

    if (changed)
        mRenderWidget->updateParameters(parameters);
}
}; // namespace IG