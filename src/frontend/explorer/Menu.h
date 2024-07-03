#pragma once

#include "Widget.h"

#include <variant>

namespace IG {
class MenuItem;
class MenuSeparator;
class Menu : public Widget {
public:
    Menu(const std::string& name);
    virtual ~Menu();

    void onWindowResize(Widget* parent, size_t width, size_t height) override;
    void onRender(Widget* parent) override;

    void add(const std::shared_ptr<Menu>& child);
    void add(const std::shared_ptr<MenuItem>& child);
    void add(const std::shared_ptr<MenuSeparator>& child);

private:
    using ChildItem = std::variant<std::shared_ptr<Menu>, std::shared_ptr<MenuItem>, std::shared_ptr<MenuSeparator>>;

    const std::string mName;
    std::vector<ChildItem> mItems;
};
} // namespace IG