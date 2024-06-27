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
        }
    }
    ImGui::End();
}

}; // namespace IG