#pragma once

#include "Widget.h"

namespace IG {
class RenderWidget : public Widget {
public:
    RenderWidget();
    virtual ~RenderWidget();

    enum class ToneMappingMethod {
        None = 0,
        Reinhard,
        ModifiedReinhard,
        ACES,
        Uncharted2
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
private:
    std::unique_ptr<class RenderWidgetInternal> mInternal;
};
} // namespace IG