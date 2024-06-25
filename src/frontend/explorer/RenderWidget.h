#pragma once

#include "Widget.h"

namespace IG {
class RenderWidget : public Widget {
public:
    RenderWidget();
    virtual ~RenderWidget();

    void openFile(const Path& path);

    void onWindowResize(Widget* parent, size_t width, size_t height) override;
    void onRender(Widget* parent) override;
    void onInput(Widget* parent) override;

private:
    std::unique_ptr<class RenderWidgetInternal> mInternal;
};
} // namespace IG