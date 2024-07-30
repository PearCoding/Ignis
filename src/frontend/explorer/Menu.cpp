#include "Menu.h"
#include "MenuItem.h"
#include "MenuSeparator.h"

#include "UI.h"

namespace IG {

Menu::Menu(const std::string& name)
    : mName(name)
{
}

Menu::~Menu()
{
}

void Menu::onWindowResize(Widget* parent, size_t width, size_t height)
{
    IG_UNUSED(parent);

    for (const auto& p : mItems) {
        if (std::holds_alternative<std::shared_ptr<Menu>>(p))
            std::get<std::shared_ptr<Menu>>(p)->onWindowResize(this, width, height);
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
        else if (std::holds_alternative<std::shared_ptr<MenuItem>>(p))
            std::get<std::shared_ptr<MenuItem>>(p)->onRender();
        else
            std::get<std::shared_ptr<MenuSeparator>>(p)->onRender();
    }

    if (parent == nullptr)
        ImGui::EndMainMenuBar();
    else
        ImGui::EndMenu();
}

void Menu::add(const std::shared_ptr<Menu>& child)
{
    mItems.emplace_back(child);
}

void Menu::add(const std::shared_ptr<MenuItem>& child)
{
    mItems.emplace_back(child);
}

void Menu::add(const std::shared_ptr<MenuSeparator>& child)
{
    mItems.emplace_back(child);
}
} // namespace IG