#pragma once

#include "Widget.h"

namespace IG {
class HelpAboutWidget : public Widget {
public:
    inline HelpAboutWidget()
        : mShow(false)
    {
    }

    void onRender(Widget* parent) override;

    inline void show(bool b = true) { mShow = b; }

private:
    bool mShow;
};
} // namespace IG