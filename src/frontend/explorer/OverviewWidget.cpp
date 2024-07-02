#include "OverviewWidget.h"
#include "MenuItem.h"
#include "RenderWidget.h"
#include "Runtime.h"

#include "imgui.h"

namespace IG {

OverviewWidget::OverviewWidget(RenderWidget* renderWidget)
    : Widget()
    , mVisibleItem(nullptr)
    , mRenderWidget(renderWidget)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

void OverviewWidget::onRender(Widget*)
{
    Runtime* runtime = mRenderWidget->currentRuntime();
    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Overview", mVisibleItem ? &visibility : nullptr)) {
        if (!runtime) {
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "No scene loaded...");
        } else {
            ImGui::Text("FPS  %.2f", mRenderWidget->currentFPS());
            ImGui::Text("Iter %zu", runtime->currentIterationCount());
            ImGui::Text("SPP  %zu", runtime->currentSampleCount());

            ImGui::Text("Lum Max %8.3f | 95%% %8.3f", runtime->parameters().getFloat("_luminance_max"), runtime->parameters().getFloat("_luminance_softmax"));
            ImGui::Text("Lum Min %8.3f |  5%% %8.3f", runtime->parameters().getFloat("_luminance_min"), runtime->parameters().getFloat("_luminance_softmin"));
            ImGui::Text("Lum Avg %8.3f | Med %8.3f", runtime->parameters().getFloat("_luminance_avg"), runtime->parameters().getFloat("_luminance_med"));

            const auto camera = runtime->getCameraOrientation();
            ImGui::Text("Cam Eye (%6.3f, %6.3f, %6.3f)", camera.Eye(0), camera.Eye(1), camera.Eye(2));
            ImGui::Text("Cam Dir (%6.3f, %6.3f, %6.3f)", camera.Dir(0), camera.Dir(1), camera.Dir(2));
            ImGui::Text("Cam Up  (%6.3f, %6.3f, %6.3f)", camera.Up(0), camera.Up(1), camera.Up(2));

            ImGui::Separator();
            ImGui::Text("DGP  %.3f", runtime->parameters().getFloat("glare_dgp"));
            ImGui::Text("Lux  %.3f", runtime->parameters().getFloat("glare_luminance"));
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);
}

}; // namespace IG