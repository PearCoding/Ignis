#include "MenuSeparator.h"

#include "imgui.h"

namespace IG {
MenuSeparator::MenuSeparator()
{
}

void MenuSeparator::onRender()
{
    ImGui::Separator();
}
} // namespace IG