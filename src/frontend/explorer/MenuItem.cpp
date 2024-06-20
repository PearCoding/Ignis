#include "MenuItem.h"

#include "imgui.h"

namespace IG {
MenuItem::MenuItem(const std::string& name, const Callback& callback)
    : mName(name)
    , mCallback(callback)
{
}

void MenuItem::onRender()
{
    if (ImGui::MenuItem(mName.c_str()))
        mCallback();
}
} // namespace IG