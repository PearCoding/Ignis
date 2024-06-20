#include "Widget.h"

#include <variant>

namespace IG {
class MenuItem;
class Menu : public Widget {
public:
    Menu(const std::string& name);
    ~Menu();

    void onResize(Widget* parent, size_t width, size_t height) override;
    void onRender(Widget* parent) override;
    void onInput(Widget* parent) override;

    void add(const std::shared_ptr<Menu>& child);
    void add(const std::shared_ptr<MenuItem>& child);

private:
    using ChildItem = std::variant<std::shared_ptr<Menu>, std::shared_ptr<MenuItem>>;

    const std::string mName;
    std::vector<ChildItem> mItems;
};
} // namespace IG