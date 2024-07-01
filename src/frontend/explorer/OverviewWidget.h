#pragma once

#include "Widget.h"

namespace IG {
class MenuItem;
class RenderWidget;
class OverviewWidget : public Widget {
public:
    OverviewWidget(RenderWidget* renderWidget);

    void onRender(Widget* parent) override;

    inline void connectMenuItem(MenuItem* item) { mVisibleItem = item; }

private:
    MenuItem* mVisibleItem;
    RenderWidget* const mRenderWidget;
};
} // namespace IG