#include "HelpControlWidget.h"

#include "UI.h"

namespace IG {
void HelpControlWidget::onRender(Widget*)
{
    static const std::string Markdown =
        R"(- *R* to reset to initial view.
- *O* to snap the up direction to the closest unit axis.
- *WASD* or arrow keys to travel through the scene.
- *Q/E* to rotate the camera around the viewing direction. 
- *PageUp/PageDown* to pan the camera up and down. 
- *Notepad +/-* to change the travel speed.
- *Numpad 1* to switch to front view.
- *Numpad 3* to switch to side view.
- *Numpad 7* to switch to top view.
- *Numpad 9* look behind you.
- *Numpad 2468* to rotate the camera.
- Mouse to rotate the camera. 
  Use with *Strg/Ctrl* to rotate the camera around the center of the scene.
  Use with *Alt* to enable first person camera behaviour.
  Use with *Strg/Ctrl* + *Alt* to rotate the camera around the center of the scene and subsequently snap the up direction.
)";

    static bool windowState = false;
    if (mShow) {
        ImGui::OpenPopup("Help - Controls", ImGuiPopupFlags_NoReopen);
        mShow = false;
        windowState = true;
    }

    ImGui::MarkdownConfig config;
    config.formatCallback = ui::markdownFormatCallback;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
    if (ImGui::BeginPopupModal("Help - Controls", &windowState)) {
        ImGui::BeginChild("#help-control-scroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
        ImGui::EndChild();

        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
} // namespace IG