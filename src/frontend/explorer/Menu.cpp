#include "Menu.h"
#include "MenuItem.h"

#include "imgui.h"

namespace IG {

Menu::Menu(const std::string& name)
    : mName(name)
{
}

Menu::~Menu()
{
}

void Menu::onResize(Widget* parent, size_t width, size_t height)
{
    IG_UNUSED(parent);

    for (const auto& p : mItems) {
        if (std::holds_alternative<std::shared_ptr<Menu>>(p))
            std::get<std::shared_ptr<Menu>>(p)->onResize(this, width, height);
    }
}

void Menu::onRender(Widget* parent)
{
    if (parent == nullptr) {
        if (!ImGui::BeginMainMenuBar())
            return;
    } else {
        if (!ImGui::BeginMenu(mName.c_str()))
            return;
    }

    for (const auto& p : mItems) {
        if (std::holds_alternative<std::shared_ptr<Menu>>(p))
            std::get<std::shared_ptr<Menu>>(p)->onRender(this);
        else
            std::get<std::shared_ptr<MenuItem>>(p)->onRender();
    }

    if (parent == nullptr)
        ImGui::EndMainMenuBar();
    else
        ImGui::EndMenu();
}

void Menu::onInput(Widget* parent)
{
    IG_UNUSED(parent);

    for (const auto& p : mItems) {
        if (std::holds_alternative<std::shared_ptr<Menu>>(p))
            std::get<std::shared_ptr<Menu>>(p)->onInput(this);
    }
}

void Menu::add(const std::shared_ptr<Menu>& child)
{
    mItems.emplace_back(child);
}

void Menu::add(const std::shared_ptr<MenuItem>& child)
{
    mItems.emplace_back(child);
}
} // namespace IG