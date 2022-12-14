#pragma once

#include "CameraProxy.h"
#include "SPPMode.h"
#include "technique/DebugMode.h"

#include <memory>

namespace IG {
enum class ToneMappingMethod {
    None = 0,
    Reinhard,
    ModifiedReinhard,
    ACES,
    Uncharted2
};

class Runtime;
class UI {
public:
    UI(SPPMode sppmode, Runtime* runtime, bool showDebug);
    ~UI();

    void setTitle(const char* str);

    enum class InputResult {
        Continue, // Continue, nothing of importance changed
        Resume,   // Resume the rendering
        Pause,    // Pause the rendering
        Reset,    // Reset the rendering
        Quit      // Quit the application
    };
    InputResult handleInput(CameraProxy& cam);
    void update();

    inline DebugMode currentDebugMode() const { return mDebugMode; }

    void setTravelSpeed(float v);

private:
    const SPPMode mSPPMode;
    DebugMode mDebugMode;

    friend class UIInternal;
    std::unique_ptr<class UIInternal> mInternal;
};
} // namespace IG