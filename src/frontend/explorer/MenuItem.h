#include "IG_Config.h"

#include <functional>

namespace IG {
class MenuItem {
public:
    using Callback = std::function<void()>;
    MenuItem(const std::string& name, const Callback& callback);

    void onRender();

private:
    std::string mName;
    Callback mCallback;
};
} // namespace IG