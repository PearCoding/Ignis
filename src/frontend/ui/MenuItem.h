#pragma once

#include "IG_Config.h"

#include <functional>

namespace IG {
class MenuItem {
public:
    using Callback = std::function<void(MenuItem*)>;
    MenuItem(const std::string& name, const Callback& callback);

    void onRender();

    inline void setSelected(bool b = true) { mSelected = b; }
    inline bool isSelected() const { return mSelected; }

private:
    std::string mName;
    bool mSelected;
    Callback mCallback;
};
} // namespace IG