#pragma once

#include "Widget.h"

namespace IG {
class RenderWidget;
class ParameterWidget : public Widget {
public:
    ParameterWidget(RenderWidget* renderWidget);

    void onRender(Widget* parent) override;

private:
    RenderWidget* const mRenderWidget;
};
} // namespace IG