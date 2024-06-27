#pragma once

#include "Widget.h"

namespace IG {
class RenderWidget;
class OverviewWidget : public Widget {
public:
    OverviewWidget(RenderWidget* renderWidget);

    void onRender(Widget* parent) override;

private:
    RenderWidget* const mRenderWidget;
};
} // namespace IG