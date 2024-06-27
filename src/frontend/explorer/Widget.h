#pragma once

#include "IG_Config.h"

namespace IG {
class Widget {
public:
    Widget()          = default;
    virtual ~Widget() = default;

    virtual void onWindowResize(Widget* parent, size_t width, size_t height) { IG_UNUSED(parent, width, height); };
    virtual void onRender(Widget* parent) = 0;
    virtual void onInput(Widget* parent) { IG_UNUSED(parent); };
};
} // namespace IG