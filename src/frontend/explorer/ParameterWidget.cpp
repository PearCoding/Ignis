#include "ParameterWidget.h"
#include "MenuItem.h"
#include "PolarSlider.h"
#include "RenderWidget.h"
#include "Runtime.h"
#include "skysun/ElevationAzimuth.h"
#include "skysun/PerezModel.h"
#include "skysun/SunLocation.h"

#include "UI.h"

namespace IG {

ParameterWidget::ParameterWidget(RenderWidget* renderWidget)
    : Widget()
    , mVisibleItem(nullptr)
    , mRenderWidget(renderWidget)
    , mKeepSizeSynced(true)
{
    IG_ASSERT(renderWidget, "Expected a valid render widget");
}

static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2", "AGX", "PbrNeutral"
};

static const char* const OverlayMethodOptions[] = {
    "None", "Luminance", "Luminance Squared", "Glare Source", "Normals", "Albedo"
};

enum class Direction {
    PositiveX = 0,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ
};
static const char* const DirectionOptions[] = {
    "+X", "-X", "+Y", "-Y", "+Z", "-Z"
};

static inline Direction fromDir(const Vector3f& d)
{
    int ind;
    d.cwiseAbs().maxCoeff(&ind);
    if (ind == 0)
        return d[ind] >= 0 ? Direction::PositiveX : Direction::NegativeX;
    else if (ind == 1)
        return d[ind] >= 0 ? Direction::PositiveY : Direction::NegativeY;
    else
        return d[ind] >= 0 ? Direction::PositiveZ : Direction::NegativeZ;
}

static inline Vector3f toDir(Direction dir)
{
    switch (dir) {
    case Direction::PositiveX:
        return Vector3f::UnitX();
    case Direction::NegativeX:
        return -Vector3f::UnitX();
    case Direction::PositiveY:
        return Vector3f::UnitY();
    case Direction::NegativeY:
        return -Vector3f::UnitY();
    default:
    case Direction::PositiveZ:
        return Vector3f::UnitZ();
    case Direction::NegativeZ:
        return -Vector3f::UnitZ();
    }
}

void ParameterWidget::onRender(Widget*)
{
    if (mVisibleItem && !mVisibleItem->isSelected())
        return;

    constexpr int HeaderFlags = ImGuiTreeNodeFlags_DefaultOpen;

    bool changed                        = false;
    RenderWidget::Parameters parameters = mRenderWidget->currentParameters();

    Runtime* runtime = mRenderWidget->currentRuntime();
    if (!runtime)
        return;

    const bool hasTonemapping = parameters.OverlayMethod == RenderWidget::OverlayMethod::None || parameters.OverlayMethod == RenderWidget::OverlayMethod::GlareSource;

    bool visibility = mVisibleItem ? mVisibleItem->isSelected() : true;
    ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Parameters", mVisibleItem ? &visibility : nullptr)) {
        if (ImGui::CollapsingHeader("View##parameters", HeaderFlags)) {
            bool fisheye = runtime->parameters().getInt("_perspective_enabled", 1) == 0;
            if (ImGui::Checkbox("Fisheye", &fisheye))
                runtime->setParameter("_perspective_enabled", (int)(fisheye ? 0 : 1));

            if (fisheye)
                ImGui::BeginDisabled();

            if (ImGui::SliderFloat("FOV", &parameters.FOV, 10.0f, 90.0f, "%.1f"))
                changed = true;

            if (fisheye)
                ImGui::EndDisabled();

            const char* current_overlay = OverlayMethodOptions[(int)parameters.OverlayMethod];
            if (ImGui::BeginCombo("Overlay", current_overlay)) {
                for (int i = 0; i < IM_ARRAYSIZE(OverlayMethodOptions); ++i) {
                    bool is_selected = (current_overlay == OverlayMethodOptions[i]);
                    if (ImGui::Selectable(OverlayMethodOptions[i], is_selected)) {
                        if (parameters.OverlayMethod != (RenderWidget::OverlayMethod)i) {
                            parameters.OverlayMethod = (RenderWidget::OverlayMethod)i;
                            changed                  = true;
                        }
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (parameters.allowColorbar()) {
                bool showColorbar = mRenderWidget->isColorbarVisible();
                if (ImGui::Checkbox("Colorbar", &showColorbar))
                    mRenderWidget->showColorbar(showColorbar);
            }

            if (!hasTonemapping)
                ImGui::BeginDisabled();

            if (ImGui::SliderFloat("Exposure", &parameters.ExposureFactor, -10.0f, 10.0f))
                changed = true;
            if (ImGui::SliderFloat("Offset", &parameters.ExposureOffset, -10.0f, 10.0f))
                changed = true;

            const char* current_method = ToneMappingMethodOptions[(int)parameters.ToneMappingMethod];
            if (ImGui::BeginCombo("Method", current_method)) {
                for (int i = 0; i < IM_ARRAYSIZE(ToneMappingMethodOptions); ++i) {
                    bool is_selected = (current_method == ToneMappingMethodOptions[i]);
                    if (ImGui::Selectable(ToneMappingMethodOptions[i], is_selected)) {
                        if (parameters.ToneMappingMethod != (RenderWidget::ToneMappingMethod)i) {
                            parameters.ToneMappingMethod = (RenderWidget::ToneMappingMethod)i;
                            changed                      = true;
                        }
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (!hasTonemapping)
                ImGui::EndDisabled();
        }

        if (mRenderWidget->currentSkyModel() == RenderWidget::SkyModel::Perez) {
            if (ImGui::CollapsingHeader("Sky##parameters", HeaderFlags)) {
                const Vector3f sun_direction = -runtime->parameters().getVector("sky_sun_dir");
                const Vector3f sky_up        = runtime->parameters().getVector("sky_up");
                ElevationAzimuth ea          = ElevationAzimuth::fromDirectionYUp(sun_direction);

                ImGui::TextUnformatted("Sun");

                constexpr float MaxElevation = 88 * Deg2Rad;
                const float item_width       = ImGui::CalcItemWidth();
                ImGui::PushItemWidth(item_width * 0.385f);
                ui::PolarSliderFloat("SunAzimuth", &ea.Azimuth);
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                ui::PolarSliderFloat("SunElevation", &ea.Elevation, MaxElevation, 0, 3);
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

                ImGui::BeginGroup();
                float azimuth_deg   = ea.Azimuth * Rad2Deg;
                float elevation_deg = ea.Elevation * Rad2Deg;
                ImGui::SliderFloat("Azimuth##SunSky", &azimuth_deg, 0, 360, "%.1f°");
                ImGui::SliderFloat("Elevation##SunSky", &elevation_deg, 0, MaxElevation * Rad2Deg, "%.1f°");
                ea.Azimuth   = azimuth_deg * Deg2Rad;
                ea.Elevation = elevation_deg * Deg2Rad;

                const auto up_opt      = fromDir(sky_up);
                const char* current_up = DirectionOptions[(int)up_opt];
                if (ImGui::BeginCombo("Up", current_up)) {
                    for (int i = 0; i < IM_ARRAYSIZE(DirectionOptions); ++i) {
                        bool is_selected = (current_up == DirectionOptions[i]);
                        if (ImGui::Selectable(DirectionOptions[i], is_selected)) {
                            if (up_opt != (Direction)i) {
                                runtime->setParameter("sky_up", toDir((Direction)i));
                                runtime->reset();
                            }
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                ImGui::EndGroup();
                ImGui::PopItemWidth();

                float day_of_the_year = runtime->parameters().getFloat("sky_day_of_the_year");
                if (ImGui::InputFloat("Day of the Year##SunSky", &day_of_the_year, 0, 360, "%.1f")) {
                    runtime->setParameter("sky_day_of_the_year", day_of_the_year);
                    runtime->reset();
                }

                static bool sunPopup = false;
                if (ImGui::Button("Load from Date")) {
                    ImGui::OpenPopup("Sun Orientation");
                    sunPopup = true;
                }

                if (ImGui::BeginPopupModal("Sun Orientation", &sunPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                    // TODO: Make this more fancy
                    static TimePoint tp;
                    static MapLocation ml;
                    static bool once = false;

                    if (!once) {
                        tp          = TimePoint::nowUTC();
                        ml          = MapLocation(); // Note: Would be cool to get the users location... but that is privacy ;)
                        ml.Timezone = 0;
                        once        = true;
                    }

                    ImGui::DragInt("Year", &tp.Year, 1.0f, 1800, 2200);
                    ImGui::DragInt("Month", &tp.Month, 1.0f, 1, 12);
                    ImGui::DragInt("Day", &tp.Day, 1.0f, 1, 31);
                    ImGui::DragInt("Hour", &tp.Hour, 1.0f, 0, 23);
                    ImGui::DragInt("Minute", &tp.Minute, 1.0f, 0, 59);
                    ImGui::DragFloat("Second", &tp.Seconds, 1.0f, 0, 59, "%.1f");
                    ImGui::DragFloat("Timezone (GMT Shift Hours)", &ml.Timezone, 1.0f, -12.0f, 12.0f);
                    ImGui::DragFloat("Longitude (West)", &ml.Longitude, 1.0f, -180.0f, 180.0f);
                    ImGui::DragFloat("Latitude (North)", &ml.Latitude, 1.0f, -90.0f, 90.0f);

                    if (ImGui::Button("Load##SunDirPopup")) {
                        ea = computeSunEA(tp, ml);
                        if (tp.dayOfTheYear() != day_of_the_year) {
                            day_of_the_year = tp.dayOfTheYear();
                            runtime->setParameter("sky_day_of_the_year", day_of_the_year);
                            runtime->reset();
                        }

                        ImGui::CloseCurrentPopup();
                        sunPopup = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Close##SunDirPopup")) {
                        ImGui::CloseCurrentPopup();
                        sunPopup = false;
                    }
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                    if (ImGui::Button("Now##SunDirPopup")) {
                        tp          = TimePoint::nowUTC();
                        ml.Timezone = 0;
                    }

                    ImGui::EndPopup();
                }

                const Vector3f new_dir = ea.toDirectionYUp();
                if (runtime->parameters().VectorParameters.contains("sky_sun_dir") /* Only update when loaded */ && !sun_direction.isApprox(new_dir, 1e-4f)) {
                    runtime->setParameter("sky_sun_dir", (-new_dir).eval());
                    runtime->reset();
                }

                ImGui::Separator();

                float sky_brightness = runtime->parameters().getFloat("sky_brightness");
                if (ImGui::SliderFloat("Brightness##Sky", &sky_brightness, 0.01f, 0.6f)) {
                    runtime->setParameter("sky_brightness", sky_brightness);
                    runtime->reset();
                }

                float sky_clearness = runtime->parameters().getFloat("sky_clearness");
                if (ImGui::SliderFloat("Clearness##Sky", &sky_clearness, 1.0f, 12.0f)) {
                    runtime->setParameter("sky_clearness", sky_clearness);
                    runtime->reset();
                }

                bool recompute_bc        = false;
                const float solar_zenith = Pi2 - ea.Elevation;
                float sky_irrad_diff     = PerezModel::computeDiffuseIrradiance(sky_brightness, solar_zenith, day_of_the_year);
                if (ImGui::SliderFloat("Diffuse Irradiance##Sky", &sky_irrad_diff, 0.01f, 1000.0f)) {
                    recompute_bc = true;
                }

                float sky_irrad_dir = PerezModel::computeDirectIrradiance(sky_brightness, sky_clearness, solar_zenith, day_of_the_year);
                if (ImGui::SliderFloat("Direct Irradiance##Sky", &sky_irrad_dir, 0.01f, 1000.0f)) {
                    recompute_bc = true;
                }

                if (recompute_bc) {
                    sky_brightness = PerezModel::computeSkyBrightness(sky_irrad_diff, solar_zenith, day_of_the_year);
                    sky_clearness  = PerezModel::computeSkyClearness(sky_irrad_diff, sky_irrad_dir, solar_zenith);
                    runtime->setParameter("sky_brightness", sky_brightness);
                    runtime->setParameter("sky_clearness", sky_clearness);
                    runtime->reset();
                }

                Vector4f sky_color = runtime->parameters().getColor("sky_color");
                if (ImGui::ColorEdit4("Color##Sky", sky_color.data())) {
                    runtime->setParameter("sky_color", sky_color);
                    runtime->reset();
                }

                Vector4f sky_ground_color = runtime->parameters().getColor("sky_ground_color");
                if (ImGui::ColorEdit4("Ground##Sky", sky_ground_color.data())) {
                    runtime->setParameter("sky_ground_color", sky_ground_color);
                    runtime->reset();
                }
            }
        }

        if (ImGui::CollapsingHeader("Glare##parameters", 0)) {
            float multiplier = runtime->parameters().getFloat("_glare_multiplier");
            if (ImGui::SliderFloat("Multiplier", &multiplier, 0.001f, 10.0f))
                runtime->setParameter("_glare_multiplier", std::max(0.001f, multiplier));
        }

        if (ImGui::CollapsingHeader("Renderer##parameters", 0)) {
            const auto internalSize = mRenderWidget->internalViewSize();

            ImGui::Checkbox("Keep equal", &mKeepSizeSynced);

            int width = (int)internalSize.first;
            ImGui::SliderInt("Width", &width, 32, 4096);

            if (mKeepSizeSynced)
                ImGui::BeginDisabled();
            int height = mKeepSizeSynced ? width : (int)internalSize.second;
            ImGui::SliderInt("Height", &height, 32, 4096);
            if (mKeepSizeSynced)
                ImGui::EndDisabled();

            if (width != (int)internalSize.first || height != (int)internalSize.second)
                mRenderWidget->resizeInternalView((size_t)std::max(32, width), (size_t)std::max(32, height));

            if (Runtime::hasDenoiser()) {
                bool useDenoiser = mRenderWidget->isDenoiserEnabled();
                if (ImGui::Checkbox("Denoise", &useDenoiser))
                    mRenderWidget->enableDenoiser(useDenoiser);
            }
        }
    }
    ImGui::End();

    if (mVisibleItem)
        mVisibleItem->setSelected(visibility);

    if (changed)
        mRenderWidget->updateParameters(parameters);
}
}; // namespace IG