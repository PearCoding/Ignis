#pragma once

#include "Widget.h"

namespace IG {
class Runtime;

class RenderWidget : public Widget {
public:
    RenderWidget();
    virtual ~RenderWidget();

    enum class ToneMappingMethod {
        None = 0,
        Reinhard,
        ModifiedReinhard,
        ACES,
        Uncharted2,
        AGX,
        PbrNeutral
    };

    struct Parameters {
        float FOV;
        float ExposureFactor;
        float ExposureOffset;
        RenderWidget::ToneMappingMethod ToneMappingMethod;
    };

    void openFile(const Path& path);

    void onRender(Widget* parent) override;
    void onInput(Widget* parent) override;

    void updateParameters(const Parameters& params);
    const Parameters& currentParameters() const;

    Runtime* currentRuntime();
    float currentFPS() const;

    bool isOverlayVisible() const;
    void showOverlay(bool b = true);

private:
    std::unique_ptr<class RenderWidgetInternal> mInternal;
};
} // namespace IG