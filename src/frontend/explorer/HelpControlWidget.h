#pragma once

#include "Widget.h"

namespace IG {
class HelpControlWidget : public Widget {
public:
    inline HelpControlWidget()
        : mShow(false)
    {
    }

    void onRender(Widget* parent) override;

    inline void show(bool b = true) { mShow = b; }

private:
    bool mShow;
};
} // namespace IG