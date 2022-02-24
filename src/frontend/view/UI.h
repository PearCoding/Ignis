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
    UI(Runtime* runtime, int width, int height, bool showDebug);
    ~UI();

    void setTitle(const char* str);
    bool handleInput(uint32_t& iter, bool& run, Camera& cam);
    void update(uint32_t iter, uint32_t samples);

    inline DebugMode currentDebugMode() const { return mDebugMode; }

    void setTravelSpeed(float v);

private:
    int mWidth;
    int mHeight;

    DebugMode mDebugMode;

    friend class UIInternal;
    std::unique_ptr<class UIInternal> mInternal;
};
} // namespace IG