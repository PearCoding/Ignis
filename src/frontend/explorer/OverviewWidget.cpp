#include "OverviewWidget.h"
#include "RenderWidget.h"
#include "Runtime.h"

#include "imgui.h"

namespace IG {

OverviewWidget::OverviewWidget(RenderWidget* renderWidget)
    : Widget()
    , mRenderWidget(renderWidget)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

void OverviewWidget::onRender(Widget*)
{
    Runtime* runtime = mRenderWidget->currentRuntime();

    if (ImGui::Begin("Overview")) {
        if (!runtime) {
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "No scene loaded...");
        } else {
            ImGui::Text("FPS  %.2f", mRenderWidget->currentFPS());
            ImGui::Text("Iter %zu", runtime->currentIterationCount());
            ImGui::Text("SPP  %zu", runtime->currentSampleCount());

            const auto camera = runtime->getCameraOrientation();

            ImGui::Text("Cam Eye (%6.3f, %6.3f, %6.3f)", camera.Eye(0), camera.Eye(1), camera.Eye(2));
            ImGui::Text("Cam Dir (%6.3f, %6.3f, %6.3f)", camera.Dir(0), camera.Dir(1), camera.Dir(2));
            ImGui::Text("Cam Up  (%6.3f, %6.3f, %6.3f)", camera.Up(0), camera.Up(1), camera.Up(2));
        }
    }
    ImGui::End();
}

}; // namespace IG