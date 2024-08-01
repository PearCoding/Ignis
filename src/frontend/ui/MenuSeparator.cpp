#include "MenuSeparator.h"

#include "UI.h"

namespace IG {
MenuSeparator::MenuSeparator()
{
}

void MenuSeparator::onRender()
{
    ImGui::Separator();
}
} // namespace IG