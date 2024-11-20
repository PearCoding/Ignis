#pragma once

#include "Widget.h"

namespace IG {
class Runtime;
class ExplorerOptions;

class RenderWidget : public Widget {
public:
    RenderWidget(const ExplorerOptions& opts);
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

    enum class OverlayMethod {
        None = 0,
        Luminance,
        LuminanceSquared,
        GlareSource,
        Normal,
        Albedo
    };

    enum class SkyModel {
        None,
        Undefined,
        Perez
    };

    struct Parameters {
        float FOV;
        float ExposureFactor;
        float ExposureOffset;
        RenderWidget::ToneMappingMethod ToneMappingMethod;
        RenderWidget::OverlayMethod OverlayMethod;

        inline bool allowColorbar() const
        {
            return OverlayMethod == OverlayMethod::Luminance || OverlayMethod == OverlayMethod::LuminanceSquared || OverlayMethod == OverlayMethod::GlareSource;
        }
    };

    void cleanup();

    void openFile(const Path& path);

    void onRender(Widget* parent) override;

    void updateParameters(const Parameters& params);
    const Parameters& currentParameters() const;

    Runtime* currentRuntime();
    float currentFPS() const;

    void resizeInternalView(size_t width, size_t height);
    std::pair<size_t, size_t> internalViewSize() const;

    bool isColorbarVisible() const;
    void showColorbar(bool b = true);

    bool isDenoiserEnabled() const;
    void enableDenoiser(bool b = true);

    SkyModel currentSkyModel() const;

private:
    std::unique_ptr<class RenderWidgetInternal> mInternal;
};
} // namespace IG