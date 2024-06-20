#include "IG_Config.h"

namespace IG {
class Widget {
public:
    Widget()          = default;
    virtual ~Widget() = default;

    virtual void onResize(Widget* parent, size_t width, size_t height) = 0;
    virtual void onRender(Widget* parent)                              = 0;
    virtual void onInput(Widget* parent)                               = 0;
};
} // namespace IG