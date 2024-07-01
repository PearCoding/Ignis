#include "HelpAboutWidget.h"
#include "config/Build.h"

#include "UI.h"

namespace IG {
static std::string prepareText()
{
    std::stringstream stream;

    stream << "Version:     " << Build::getVersionString() << std::endl
           << "Build:       " << Build::getBuildVariant() << std::endl
           << "Git Rev:     " << Build::getGitRevision() << std::endl
           << "Git Branch:  " << Build::getGitBranch() << std::endl
           << "Git Dirty:   " << (Build::isGitDirty() ? "true" : "false") << std::endl
           << "Compiler:    " << Build::getCompilerName() << std::endl
           << "OS:          " << Build::getOSName() << std::endl
           << "Definitions: " << Build::getBuildDefinitions() << std::endl
           << "Options:     " << Build::getBuildOptions() << std::endl;

    return stream.str();
}

void HelpAboutWidget::onRender(Widget*)
{
    static std::string Markdown;
    if (Markdown.empty())
        Markdown = prepareText();

    static bool windowState = false;
    if (mShow) {
        ImGui::OpenPopup("About Ignis", ImGuiPopupFlags_NoReopen);
        mShow       = false;
        windowState = true;
    }

    ImGui::MarkdownConfig config;
    config.formatCallback = ui::markdownFormatCallback;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
    if (ImGui::BeginPopupModal("About Ignis", &windowState)) {
        ImGui::BeginChild("#help-about-scroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
        ImGui::EndChild();

        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button("Copy"))
            SDL_SetClipboardText(Markdown.c_str());
        ImGui::EndPopup();
    }
}
} // namespace IG