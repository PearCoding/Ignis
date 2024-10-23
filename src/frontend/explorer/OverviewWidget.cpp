#include "OverviewWidget.h"
#include "MenuItem.h"
#include "RenderWidget.h"
#include "Runtime.h"

#include "UI.h"

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
            if (runtime->currentIterationCount() != runtime->currentSampleCount())
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

            const float dgp    = runtime->parameters().getFloat("glare_dgp");
            const float dgi    = runtime->parameters().getFloat("glare_dgi");
            const float dgiMod = runtime->parameters().getFloat("glare_dgi_mod");
            const float dgr    = runtime->parameters().getFloat("glare_dgr");
            const float vcp    = runtime->parameters().getFloat("glare_vcp");
            const float ugr    = runtime->parameters().getFloat("glare_ugr");
            const float ugrExp = runtime->parameters().getFloat("glare_ugr_exp");
            const float ugp    = runtime->parameters().getFloat("glare_ugp");

            const char* dgpPerception = "Imperceptible";
            if (dgp > 0.45f)
                dgpPerception = "Intolerable";
            else if (dgp > 0.40f)
                dgpPerception = "Disturbing";
            else if (dgp > 0.35f)
                dgpPerception = "Perceptible";

            ImGui::Text("DGP  %.3f [%s]", dgp, dgpPerception);
            ImGui::Text("DGI  %.3f", dgi);
            ImGui::Text("DGIm %.3f", dgiMod);
            ImGui::Text("DGR  %.3f", dgr);
            ImGui::Text("VCP  %.3f", vcp);
            ImGui::Text("UGR  %.3f", ugr);
            ImGui::Text("UGRe %.3f", ugrExp);
            ImGui::Text("UGP  %.3f", ugp);

            ImGui::Separator();

            ImGui::Text("Ev   %.3f lx", runtime->parameters().getFloat("glare_vertical_illuminance"));
            ImGui::Text("Lv   %.3f lx", runtime->parameters().getFloat("glare_source_luminance"));
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);
}

}; // namespace IG