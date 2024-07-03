#pragma once

#include "Widget.h"

namespace IG {
class MenuItem;
class RenderWidget;
class ParameterWidget : public Widget {
public:
    ParameterWidget(RenderWidget* renderWidget);

    void onRender(Widget* parent) override;

    inline void connectMenuItem(MenuItem* item) { mVisibleItem = item; }

private:
    MenuItem* mVisibleItem;
    RenderWidget* const mRenderWidget;

    bool mKeepSizeSynced;
};
} // namespace IG