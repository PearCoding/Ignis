#pragma once

#include "Camera.h"
#include "DebugMode.h"

#include <memory>

namespace IG {
enum class ToneMappingMethod {
    None = 0,
    Reinhard,
    ModifiedReinhard,
    ACES
};

class Runtime;
class UI {
public:
    UI(Runtime* runtime, size_t width, size_t height, bool showDebug);
    ~UI();

    void setTitle(const char* str);
    bool handleInput(size_t& iter, bool& run, Camera& cam);
    void update(size_t iter, size_t samples);

    inline DebugMode currentDebugMode() const { return mDebugMode; }

    void setTravelSpeed(float v);

private:
    size_t mWidth;
    size_t mHeight;

    DebugMode mDebugMode;

    friend class UIInternal;
    std::unique_ptr<class UIInternal> mInternal;
};
} // namespace IG