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
class Context {
public:
    Context(SPPMode sppmode, Runtime* runtime, bool showDebug, float dpi);
    ~Context();

    void setTitle(const char* str);

    enum class InputResult {
        Continue, // Continue, nothing of importance changed
        Resume,   // Resume the rendering
        Pause,    // Pause the rendering
        Reset,    // Reset the rendering
        Quit      // Quit the application
    };
    [[nodiscard]] InputResult handleInput(CameraProxy& cam);

    enum class UpdateResult {
        Continue, // Continue, nothing of importance changed
        Reset     // Reset the rendering
    };
    [[nodiscard]] UpdateResult update();

    [[nodiscard]] inline DebugMode currentDebugMode() const { return mDebugMode; }

    void setTravelSpeed(float v);

private:
    const SPPMode mSPPMode;
    DebugMode mDebugMode;

    friend class ContextInternal;
    std::unique_ptr<class ContextInternal> mInternal;
};
} // namespace IG