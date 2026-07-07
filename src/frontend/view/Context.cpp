#include "Context.h"
#include "IO.h"
#include "Pose.h"
#include "Runtime.h"

#include "GLTexture.h"
#include "Inspector.h"
#include "PropertyView.h"
#include "UI.h"

#include "Color.h"
#include "Logger.h"

#include <algorithm>

namespace IG {

using namespace ui;

constexpr size_t HISTOGRAM_SIZE                     = 50;
static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2", "AGX", "PbrNeutral"
};

// Pose IO
constexpr const char* const POSE_FILE = "poses.lst";

struct LuminanceInfo {
    float Min     = FltInf;
    float Max     = 0.0f;
    float Avg     = 0.0f;
    float SoftMin = FltInf;
    float SoftMax = 0.0f;
    float Med     = 0.0f;
    float Est     = 1e-5f;
    int InfCount  = 0;
    int NaNCount  = 0;
    int NegCount  = 0;

    LuminanceInfo& operator=(const LuminanceInfo& other) = default;
};

enum class ScreenshotRequestMode {
    Nothing,
    Framebuffer,
    Full
};

class ContextInternal {
public:
    IG::Runtime* Runtime = nullptr;
    Context* Parent      = nullptr;
    GLFWwindow* Window   = nullptr;
    std::unique_ptr<ui::GLTexture> Texture;
    std::vector<uint32_t> Buffer;

    // Placement of the rendered image inside the ImGui "Render" window (screen space)
    ImVec2 RenderImageMin  = ImVec2(0, 0);
    ImVec2 RenderImageSize = ImVec2(0, 0);
    bool RenderHovered     = false;

    int PoseRequest                         = -1;
    bool PoseResetRequest                   = false;
    ScreenshotRequestMode ScreenshotRequest = ScreenshotRequestMode::Nothing;
    bool ShowHelp                           = false;
    bool ShowControl                        = true;
    bool ShowProperties                     = false;
    bool ShowInspector                      = false;
    bool LockInteraction                    = false;
    bool ZoomIsScale                        = false;

    size_t Width = 0, Height = 0;

    // Stats
    LuminanceInfo LastLum;
    std::array<int, HISTOGRAM_SIZE * 4> Histogram;

    bool ToneMapping_Automatic              = false;
    float ToneMapping_Exposure              = 0.0f;
    float ToneMapping_Offset                = 0.0f;
    bool ToneMappingGamma                   = true;
    IG::ToneMappingMethod ToneMappingMethod = ToneMappingMethod::PbrNeutral;

    std::string CurrentAOV = "Color";

    bool Running = true;

    IG::PoseManager PoseManager;
    CameraPose LastCameraPose;

    float CurrentTravelSpeed = 1.0f;
    float CurrentZoom        = 1.0f; // Only important if orthogonal
    float DefaultCameraScale = 1.0f;

    inline bool isAnyWindowShown() const { return ShowControl || ShowProperties || ShowInspector || ShowHelp; }

    // Buffer stuff
    bool setupTextureBuffer(size_t width, size_t height)
    {
        if (!Texture)
            Texture = std::make_unique<ui::GLTexture>();
        Texture->resize((int)width, (int)height);

        Buffer.resize(width * height);
        return true;
    }

    // Events
    void handlePoseInput(size_t posenmbr, bool capture, const CameraProxy& cam)
    {
        if (!capture) {
            PoseRequest = (int)posenmbr;
        } else {
            PoseManager.setPose(posenmbr, CameraPose(cam));
            IG_LOG(L_INFO) << "Captured pose for " << posenmbr + 1 << std::endl;
        }
    }

    bool handleFramebufferResize(int width, int height)
    {
        // Expect a useful minimum!
        if (width <= 5 || height <= 5)
            return false;

        // Check if something changed
        if (Width == (size_t)width && Height == (size_t)height)
            return false;

        Runtime->resizeFramebuffer((size_t)width, (size_t)height);
        setupTextureBuffer((size_t)width, (size_t)height);
        Width  = width;
        Height = height;
        return true;
    }

    // Draw the rendered framebuffer into a dockable "Render" window and remember its
    // on-screen placement (for the inspector). Returns true if the framebuffer was
    // resized to match the window, in which case the render should reset.
    bool drawRenderWindow()
    {
        RenderHovered   = false;
        RenderImageSize = ImVec2(0, 0);

        bool resized = false;
        if (ImGui::Begin("Render", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            resized            = handleFramebufferResize((int)avail.x, (int)avail.y);

            updateSurface(); // tonemap the current framebuffer into the GL texture

            if (Texture && Width > 0 && Height > 0) {
                RenderImageMin  = ImGui::GetCursorScreenPos();
                RenderImageSize = ImVec2((float)Width, (float)Height);
                ImGui::Image(Texture->id(), RenderImageSize);
                RenderHovered = ImGui::IsItemHovered();
            }
        }
        ImGui::End();
        return resized;
    }

    [[nodiscard]] inline AOVAccessor currentPixels() const
    {
        return Runtime->getFramebufferForHost(CurrentAOV);
    }

    void changeAOV(ptrdiff_t delta_aov)
    {
        const auto names = Runtime->framebufferNames();
        if (names.empty())
            return;

        const ptrdiff_t pos = std::distance(names.begin(), std::find(names.begin(), names.end(), CurrentAOV));
        const ptrdiff_t rem = (ptrdiff_t)names.size();
        CurrentAOV          = names.at(((pos + delta_aov) % rem + rem) % rem);
    }

    // Events
    Context::InputResult handleEvents(CameraProxy& cam)
    {
        const Vector3f sceneCenter = Runtime->sceneBoundingBox().center();

        static bool first_call = true;
        if (first_call) {
            PoseManager.setInitalPose(CameraPose(cam));
            first_call = false;
        }

        // Pump window events and begin the ImGui frame. Camera input is polled from
        // ImGui IO below; the actual widgets are drawn later in update().
        glfwPollEvents();
        ui::newFrame();
        ImGui::DockSpaceOverViewport();

        if (glfwWindowShouldClose(Window))
            return Context::InputResult::Quit;

        ImGuiIO& io = ImGui::GetIO();

        constexpr float RSPEED  = 0.005f;
        constexpr float KRSPEED = 10 * RSPEED;

        const bool canInteract = !LockInteraction && Running;
        const bool keyActive   = canInteract && !io.WantTextInput;
        // The mouse only drives the camera while hovering the rendered image
        // (state captured last frame while drawing the "Render" window).
        const bool mouseActive = canInteract && RenderHovered && !io.WantTextInput;

        const auto handleRotation = [&](float xmotion, float ymotion) {
            if (io.KeyCtrl && io.KeyAlt) {
                cam.rotate_around(sceneCenter, xmotion, ymotion);
                cam.snap_up();
            } else if (io.KeyAlt) {
                cam.rotate_fixroll(xmotion, ymotion);
            } else if (io.KeyCtrl) {
                cam.rotate_around(sceneCenter, xmotion, ymotion);
            } else {
                cam.rotate(xmotion, ymotion);
            }
        };

        bool reset = false;

        // --- One-shot keys: quit / tonemapping / AOV / pause (blocked while typing) ---
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            return Context::InputResult::Quit;

        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false))
                ToneMapping_Automatic = !ToneMapping_Automatic;
            if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !ToneMapping_Automatic) {
                ToneMapping_Exposure = 0.0f;
                ToneMapping_Offset   = 0.0f;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !ToneMapping_Automatic) {
                const float delta = io.KeyCtrl ? 0.05f : 0.5f;
                ToneMapping_Exposure += io.KeyShift ? -delta : delta;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V, false) && !ToneMapping_Automatic) {
                const float delta = io.KeyCtrl ? 0.05f : 0.5f;
                ToneMapping_Offset += io.KeyShift ? -delta : delta;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_N, false))
                changeAOV(-1);
            if (ImGui::IsKeyPressed(ImGuiKey_M, false))
                changeAOV(1);
            if (ImGui::IsKeyPressed(ImGuiKey_I, false))
                ShowInspector = !ShowInspector;
            if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
                if (Running) {
                    Running = false;
                    return Context::InputResult::Pause;
                } else {
                    Running = true;
                    return Context::InputResult::Resume;
                }
            }
        }

        // --- Window / lock toggles (always active) ---
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            ShowHelp = !ShowHelp;
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            ShowControl = !ShowControl;
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
            LockInteraction = !LockInteraction;
        if (ImGui::IsKeyPressed(ImGuiKey_F4, false))
            ShowProperties = !ShowProperties;
        if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
            ScreenshotRequest = io.KeyCtrl ? ScreenshotRequestMode::Full : ScreenshotRequestMode::Framebuffer;

        // --- One-shot view directions and pose slots ---
        if (keyActive) {
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) {
                cam.update_dir(Vector3f(0, 0, 1), Vector3f(0, 1, 0));
                reset = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false)) {
                cam.update_dir(Vector3f(1, 0, 0), Vector3f(0, 1, 0));
                reset = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false)) {
                cam.update_dir(Vector3f(0, 1, 0), Vector3f(0, 0, 1));
                reset = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad9, false)) {
                cam.update_dir(-cam.Direction, cam.Up);
                reset = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
                cam.snap_up();
                reset = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_R, false))
                PoseResetRequest = true;

            const bool capture                     = io.KeyCtrl;
            static constexpr ImGuiKey NumberKeys[] = { ImGuiKey_0, ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5, ImGuiKey_6, ImGuiKey_7, ImGuiKey_8, ImGuiKey_9 };
            for (int i = 0; i < 10; ++i) {
                if (ImGui::IsKeyPressed(NumberKeys[i], false))
                    handlePoseInput((size_t)((i + 9) % 10), capture, cam); // keys 1..9 -> slots 0..8, key 0 -> slot 9
            }
        }

        // --- Continuous movement / rotation (held keys) ---
        if (keyActive) {
            const auto down = [](ImGuiKey k) { return ImGui::IsKeyDown(k); };

            if (down(ImGuiKey_W) || down(ImGuiKey_UpArrow)) {
                cam.move(0, 0, CurrentTravelSpeed);
                reset = true;
            }
            if (down(ImGuiKey_S) || down(ImGuiKey_DownArrow)) {
                cam.move(0, 0, -CurrentTravelSpeed);
                reset = true;
            }
            if (down(ImGuiKey_A) || down(ImGuiKey_LeftArrow)) {
                cam.move(-CurrentTravelSpeed, 0, 0);
                reset = true;
            }
            if (down(ImGuiKey_D) || down(ImGuiKey_RightArrow)) {
                cam.move(CurrentTravelSpeed, 0, 0);
                reset = true;
            }
            if (down(ImGuiKey_E)) {
                cam.roll(KRSPEED);
                reset = true;
            }
            if (down(ImGuiKey_Q)) {
                cam.roll(-KRSPEED);
                reset = true;
            }
            if (down(ImGuiKey_PageUp)) {
                cam.move(0, CurrentTravelSpeed, 0);
                reset = true;
            }
            if (down(ImGuiKey_PageDown)) {
                cam.move(0, -CurrentTravelSpeed, 0);
                reset = true;
            }
            if (down(ImGuiKey_Keypad2)) {
                handleRotation(0, KRSPEED);
                reset = true;
            }
            if (down(ImGuiKey_Keypad8)) {
                handleRotation(0, -KRSPEED);
                reset = true;
            }
            if (down(ImGuiKey_Keypad4)) {
                handleRotation(-KRSPEED, 0);
                reset = true;
            }
            if (down(ImGuiKey_Keypad6)) {
                handleRotation(KRSPEED, 0);
                reset = true;
            }
            if (down(ImGuiKey_KeypadAdd))
                CurrentTravelSpeed *= 1.1f;
            if (down(ImGuiKey_KeypadSubtract))
                CurrentTravelSpeed *= 0.9f;
        }

        // --- Mouse look / pan / zoom over the rendered image ---
        if (mouseActive) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                handleRotation(d.x * RSPEED, d.y * RSPEED);
                reset = true;
            } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                const ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0.0f);
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
                const float aspeed = CurrentTravelSpeed / 10;
                cam.move(d.x * aspeed, -d.y * aspeed, 0);
                reset = true;
            }

            if (io.MouseWheel != 0) {
                if (ZoomIsScale)
                    CurrentZoom *= (io.MouseWheel < 0) ? -io.MouseWheel * 1.05f : io.MouseWheel * 0.95f;
                else
                    cam.move(0, 0, io.MouseWheel * CurrentTravelSpeed);
                reset = true;
            }
        }

        if (canInteract && (PoseResetRequest || PoseRequest >= 0)) {
            auto pose = PoseResetRequest ? PoseManager.initialPose() : PoseManager.pose(PoseRequest);
            cam.Eye   = pose.Eye;
            cam.update_dir(pose.Dir, pose.Up);
            CurrentZoom      = 1;
            PoseRequest      = -1;
            PoseResetRequest = false;

            reset = true;
        }

        LastCameraPose = CameraPose(cam);
        if (Running && ZoomIsScale)
            Runtime->setParameter("__camera_scale", DefaultCameraScale * CurrentZoom);

        return reset ? Context::InputResult::Reset : Context::InputResult::Continue;
    }

    inline float getIterationScale() const
    {
        return Runtime->currentIterationCount() > 0 ? 1.0f / Runtime->currentIterationCount() : 1.0f;
    }

    // Mouse position in framebuffer pixel space, or (-1,-1) if outside the render image.
    void imageMousePixel(int& px, int& py) const
    {
        px = py = -1;
        if (RenderImageSize.x <= 0 || RenderImageSize.y <= 0)
            return;

        const ImVec2 m = ImGui::GetMousePos();
        const float u  = (m.x - RenderImageMin.x) / RenderImageSize.x;
        const float v  = (m.y - RenderImageMin.y) / RenderImageSize.y;
        if (u < 0 || u >= 1 || v < 0 || v >= 1)
            return;

        px = (int)(u * (float)Width);
        py = (int)(v * (float)Height);
    }

    void analyzeLuminance()
    {
        ImageInfoSettings settings{
            .AOV               = CurrentAOV.c_str(),
            .Scale             = getIterationScale(),
            .Bins              = HISTOGRAM_SIZE,
            .HistogramR        = Histogram.data() + 0 * HISTOGRAM_SIZE,
            .HistogramG        = Histogram.data() + 1 * HISTOGRAM_SIZE,
            .HistogramB        = Histogram.data() + 2 * HISTOGRAM_SIZE,
            .HistogramL        = Histogram.data() + 3 * HISTOGRAM_SIZE,
            .AcquireErrorStats = true,
            .AcquireHistogram  = true
        };

        const ImageInfoOutput output = Runtime->imageinfo(settings);

        LastLum          = LuminanceInfo();
        LastLum.Avg      = output.Average;
        LastLum.Max      = output.Max;
        LastLum.Min      = output.Min;
        LastLum.Med      = output.Median;
        LastLum.SoftMax  = output.SoftMax;
        LastLum.SoftMin  = output.SoftMin;
        LastLum.Est      = output.SoftMax;
        LastLum.InfCount = output.InfCount;
        LastLum.NaNCount = output.NaNCount;
        LastLum.NegCount = output.NegCount;
    }

    void updateSurface()
    {
        analyzeLuminance();

        // TODO: It should be possible to directly change the device buffer (if the computing device is the display device)... but thats very advanced
        uint32* buf = Buffer.data();
        IG_ASSERT(buf != nullptr, "Expected valid buffer");

        Runtime->tonemap(buf,
                         TonemapSettings{
                             .AOV            = CurrentAOV.c_str(),
                             .Method         = (size_t)ToneMappingMethod,
                             .UseGamma       = ToneMappingGamma,
                             .Scale          = getIterationScale(),
                             .ExposureFactor = ToneMapping_Automatic ? 1 / LastLum.Est : std::pow(2.0f, ToneMapping_Exposure),
                             .ExposureOffset = ToneMapping_Automatic ? 0 : ToneMapping_Offset });

        Texture->update(buf);
    }

    [[nodiscard]] inline Color getFilmData(size_t width, size_t height, uint32_t x, uint32_t y)
    {
        IG_UNUSED(height);
        const auto acc    = currentPixels();
        const float* film = acc.Data;
        const float scale = (acc.Flags & AOVFlags::Once) == AOVFlags::Once || (acc.Flags & AOVFlags::Snapshot) == AOVFlags::Snapshot ? 1.0f : getIterationScale();
        const size_t ind  = y * width + x;

        return Color(
            film[ind * 3 + 0] * scale,
            film[ind * 3 + 1] * scale,
            film[ind * 3 + 2] * scale);
    }

    void makeScreenshot()
    {
        std::stringstream out_file;
        auto now       = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        out_file << "screenshot_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

        if (!Runtime->saveFramebuffer(out_file.str()))
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
        else
            IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;
    }

    void makeFullScreenshot()
    {
        std::stringstream out_file;
        auto now       = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        out_file << "screenshot_full_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

        // Read back the composited window (image + UI) from the front buffer.
        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(Window, &fbw, &fbh);
        if (fbw <= 0 || fbh <= 0)
            return;

        std::vector<uint8_t> pixels((size_t)fbw * (size_t)fbh * 4);
        glReadBuffer(GL_FRONT);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        float* rgba = new float[(size_t)fbw * (size_t)fbh * 4];
        for (int y = 0; y < fbh; ++y) {
            // OpenGL's origin is bottom-left, so flip vertically into the output.
            const uint8* src = pixels.data() + (size_t)(fbh - 1 - y) * (size_t)fbw * 4;
            float* dst       = rgba + (size_t)y * (size_t)fbw * 4;
            for (int x = 0; x < fbw; ++x) {
                // EXR is linear, so remove gamma part
                dst[x * 4 + 0] = std::pow(src[x * 4 + 0] / 255.0f, 2.2f);
                dst[x * 4 + 1] = std::pow(src[x * 4 + 1] / 255.0f, 2.2f);
                dst[x * 4 + 2] = std::pow(src[x * 4 + 2] / 255.0f, 2.2f);
                dst[x * 4 + 3] = src[x * 4 + 3] / 255.0f; // Do not map alpha channel (which should be 1 99% of the time)
            }
        }

        if (!saveImageRGBA(out_file.str(), rgba, (size_t)fbw, (size_t)fbh, 1))
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
        else
            IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;

        delete[] rgba;
    }

    static constexpr int WindowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
    void handleControlWindow()
    {
        constexpr size_t UI_W   = 400;
        constexpr size_t UI_H   = 665;
        constexpr size_t HIST_W = 250;

        ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(UI_W, UI_H), ImGuiCond_Once);
        if (ImGui::Begin("Control", &ShowControl, WindowFlags)) {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                int mouse_x, mouse_y;
                imageMousePixel(mouse_x, mouse_y);
                Color rgb;
                if (mouse_x >= 0 && mouse_y >= 0)
                    rgb = getFilmData(Width, Height, (uint32)mouse_x, (uint32)mouse_y);

                ImGui::Text("Iter %zu", Runtime->currentIterationCount());
                ImGui::Text("SPP  %zu", Runtime->currentSampleCount());
                if (Parent->mSPPMode == SPPMode::Continuous)
                    ImGui::Text("Frame %zu", Runtime->currentFrameCount());
                ImGui::Text("Cursor  (%f, %f, %f)", rgb.r, rgb.g, rgb.b);
                ImGui::Text("Lum Max %8.3f | 95%% %8.3f", LastLum.Max, LastLum.SoftMax);
                ImGui::Text("Lum Min %8.3f |  5%% %8.3f", LastLum.Min, LastLum.SoftMin);
                ImGui::Text("Lum Avg %8.3f | Med %8.3f", LastLum.Avg, LastLum.Med);

                // Draw informative section
                if (LastLum.InfCount > 0 || LastLum.NaNCount > 0 || LastLum.NegCount > 0) {
                    const size_t pixel_comp_count = Width * Height * 3;
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 0, 0, 255));
                    if (LastLum.InfCount > 0)
                        ImGui::Text("Infinite %7.3f%%", 100 * LastLum.InfCount / (float)pixel_comp_count);
                    if (LastLum.NaNCount > 0)
                        ImGui::Text("NaN     %8.3f%%", 100 * LastLum.NaNCount / (float)pixel_comp_count);
                    if (LastLum.NegCount > 0)
                        ImGui::Text("Negative %7.3f%%", 100 * LastLum.NegCount / (float)pixel_comp_count);
                    ImGui::PopStyleColor();
                }

                ImGui::Text("Cam Eye (%6.3f, %6.3f, %6.3f)", LastCameraPose.Eye(0), LastCameraPose.Eye(1), LastCameraPose.Eye(2));
                ImGui::Text("Cam Dir (%6.3f, %6.3f, %6.3f)", LastCameraPose.Dir(0), LastCameraPose.Dir(1), LastCameraPose.Dir(2));
                ImGui::Text("Cam Up  (%6.3f, %6.3f, %6.3f)", LastCameraPose.Up(0), LastCameraPose.Up(1), LastCameraPose.Up(2));

                ImGui::PushItemWidth(-1);
                ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
                if (ImPlot::BeginPlot("Histogram", ImVec2((int)HIST_W, 100), ImPlotFlags_NoTitle | ImPlotFlags_NoInputs | ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus)) {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels);
                    ImPlot::SetupAxesLimits(0, (double)HISTOGRAM_SIZE, 0, static_cast<double>(Width * Height), ImPlotCond_Always);
                    ImPlot::SetupFinish();

                    constexpr double BarWidth = 0.67;
                    // ImPlot 1.1 replaced SetNextLineStyle/SetNextFillStyle with a per-item ImPlotSpec.
                    const auto barSpec = [](const ImVec4& color) {
                        ImPlotSpec spec;
                        spec.LineWeight = 0; // No bar edges
                        spec.FillColor  = color;
                        spec.FillAlpha  = 0.25f;
                        return spec;
                    };
                    ImPlot::PlotBars("R", Histogram.data() + 0 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, barSpec(ImVec4(1, 0, 0, 1)));
                    ImPlot::PlotBars("G", Histogram.data() + 1 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, barSpec(ImVec4(0, 1, 0, 1)));
                    ImPlot::PlotBars("B", Histogram.data() + 2 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, barSpec(ImVec4(0, 0, 1, 1)));
                    ImPlot::PlotBars("L", Histogram.data() + 3 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, barSpec(ImVec4(1, 1, 0, 1)));

                    ImPlot::EndPlot();
                }
                ImPlot::PopStyleVar();
                ImGui::PopItemWidth();
            }

            const auto aovNames = Runtime->framebufferNames();
            if (!aovNames.empty()) {
                if (ImGui::CollapsingHeader("AOV", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const char* current_aov = CurrentAOV.c_str();
                    if (ImGui::BeginCombo("Display", current_aov)) {
                        for (size_t i = 0; i < aovNames.size(); ++i) {
                            const std::string& name = aovNames.at(i).c_str();
                            bool is_selected        = (name == CurrentAOV);
                            if (ImGui::Selectable(name.c_str(), is_selected))
                                CurrentAOV = name;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }
                }
            }

            if (ImGui::CollapsingHeader("ToneMapping", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Automatic", &ToneMapping_Automatic);
                if (!ToneMapping_Automatic) {
                    ImGui::SliderFloat("Exposure", &ToneMapping_Exposure, -10.0f, 10.0f);
                    ImGui::SliderFloat("Offset", &ToneMapping_Offset, -10.0f, 10.0f);
                }

                const char* current_method = ToneMappingMethodOptions[(int)ToneMappingMethod];
                if (ImGui::BeginCombo("Method", current_method)) {
                    for (int i = 0; i < IM_ARRAYSIZE(ToneMappingMethodOptions); ++i) {
                        bool is_selected = (current_method == ToneMappingMethodOptions[i]);
                        if (ImGui::Selectable(ToneMappingMethodOptions[i], is_selected))
                            ToneMappingMethod = (IG::ToneMappingMethod)i;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Gamma", &ToneMappingGamma);
            }

            if (ImGui::CollapsingHeader("Poses")) {
                if (ImGui::Button("Reload")) {
                    PoseManager.load(POSE_FILE);
                    IG_LOG(L_INFO) << "Poses loaded from '" << POSE_FILE << "'" << std::endl;
                }
                ImGui::SameLine();
                if (ImGui::Button("Save")) {
                    PoseManager.save(POSE_FILE);
                    IG_LOG(L_INFO) << "Poses saved to '" << POSE_FILE << "'" << std::endl;
                }

                bool f = false;
                for (size_t i = 0; i < PoseManager.poseCount(); ++i) {
                    const auto pose = PoseManager.pose(i);
                    std::stringstream sstream;
                    sstream << i + 1 << " | " << pose.Eye(0) << " " << pose.Eye(1) << " " << pose.Eye(2);
                    if (ImGui::Selectable(sstream.str().c_str(), &f))
                        PoseRequest = (int)i;
                }
            }
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Press F1 for help...");
        }
        ImGui::End();

        // Disable annoying initial focus
        static bool once = false;
        if (!once) {
            ImGui::SetWindowFocus(nullptr);
            once = true;
        }
    }

    Context::UpdateResult handlePropertyWindow()
    {
        constexpr size_t PROP_W      = 350;
        constexpr size_t PROP_H      = 400;
        Context::UpdateResult result = Context::UpdateResult::Continue;
        ImGui::SetNextWindowPos(ImVec2((float)(Width - 5 - PROP_W), 5.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2((float)PROP_W, (float)PROP_H), ImGuiCond_Once);
        if (ImGui::Begin("Properties", &ShowProperties, WindowFlags)) {
            if (ui_registry_view(Runtime))
                result = Context::UpdateResult::Reset;
        }
        ImGui::End();
        return result;
    }

    Context::UpdateResult handleImgui()
    {
        if (ShowControl)
            handleControlWindow();

        Context::UpdateResult result = Context::UpdateResult::Continue;
        if (ShowProperties)
            result = handlePropertyWindow();

        if (ShowInspector) {
            int mouse_x, mouse_y;
            imageMousePixel(mouse_x, mouse_y);
            const auto acc = currentPixels();
            ui_inspect_image(mouse_x, mouse_y, Width, Height, getIterationScale(), acc.Data, Buffer.data());
        }

        return result;
    }
};

////////////////////////////////////////////////////////////////

Context::Context(SPPMode sppmode, Runtime* runtime, float dpi)
    : mSPPMode(sppmode)
    , mInternal(std::make_unique<ContextInternal>())
{
    mInternal->Runtime     = runtime;
    mInternal->Parent      = this;
    mInternal->Width       = runtime->framebufferWidth();
    mInternal->Height      = runtime->framebufferHeight();
    mInternal->ZoomIsScale = runtime->camera() == "orthogonal";

    if (auto it = runtime->parameters().FloatParameters.find("__camera_scale"); it != runtime->parameters().FloatParameters.end())
        mInternal->DefaultCameraScale = it->second;

    if (!ui::setup(mInternal->Window, (int)mInternal->Width, (int)mInternal->Height, "Ignis", true, dpi))
        throw std::runtime_error("Could not setup UI");

    // Requires an active GL context (created by ui::setup above)
    if (!mInternal->setupTextureBuffer(mInternal->Width, mInternal->Height))
        throw std::runtime_error("Could not setup UI");

    mInternal->PoseManager.load(POSE_FILE);

    mInternal->ShowProperties = runtime->hasSceneParameters();

    if ((float)mInternal->Width < 450 || (float)mInternal->Height < 600) {
        IG_LOG(L_WARNING) << "Window too small to show UI. Hiding it by default. Press F2 or F4 to show it" << std::endl;
        mInternal->ShowControl    = false;
        mInternal->ShowProperties = false;
    }
}

Context::~Context()
{
    // The GL texture must be released while the context is still current.
    mInternal->Texture.reset();
    ui::shutdown(mInternal->Window);
    mInternal->Buffer.clear();
}

void Context::setTitle(const std::string& str)
{
    std::stringstream sstream;

    sstream << str;
    if (mInternal->LockInteraction)
        sstream << " [Locked]";

    switch (mSPPMode) {
    default:
    case SPPMode::Fixed:
        break;
    case SPPMode::Capped:
        sstream << " [Capped]";
        break;
    case SPPMode::Continuous:
        sstream << " [Continuous]";
        break;
    }
    glfwSetWindowTitle(mInternal->Window, sstream.str().c_str());
}

Context::InputResult Context::handleInput(CameraProxy& cam)
{
    return mInternal->handleEvents(cam);
}

static void handleHelp()
{
    static const std::string Markdown =
        R"(- *1..9* number keys to switch between views.
- *1..9* and *Strg/Ctrl* to save the current view on that slot.
- *F1* to toggle this help window.
- *F2* to toggle the control window.
- *F3* to toggle the interaction lock. 
- *F4* to toggle the properties window.
  If enabled, no view changing interaction is possible.
- *F11* to save a snapshot of the current rendering. HDR information will be preserved.
  Use with *Strg/Ctrl* to make a LDR screenshot of the current render including UI and tonemapping.  
  The image will be saved in the current working directory.
- *I* to toggle the inspector tool.
- *R* to reset to initial view.
- *O* to snap the up direction to the closest unit axis.
- *P* to pause current rendering. Also implies an interaction lock.
- *T* to toggle automatic tonemapping.
- *G* to reset tonemapping properties.
  Only works if automatic tonemapping is disabled.
- *F* to increase (or with *Shift* to decrease) tonemapping exposure.
  Step size can be decreased with *Strg/Ctrl*.
  Only works if automatic tonemapping is disabled.
- *V* to increase (or with *Shift* to decrease) tonemapping offset.
  Step size can be decreased with *Strg/Ctrl*.
  Only works if automatic tonemapping is disabled.
- *N/M* to switch to previous or next available AOV. 
- *WASD* or arrow keys to travel through the scene.
- *Q/E* to rotate the camera around the viewing direction. 
- *PageUp/PageDown* to pan the camera up and down. 
- *Notepad +/-* to change the travel speed.
- *Numpad 1* to switch to front view.
- *Numpad 3* to switch to side view.
- *Numpad 7* to switch to top view.
- *Numpad 9* look behind you.
- *Numpad 2468* to rotate the camera.
- Mouse to rotate the camera. 
  Use with *Strg/Ctrl* to rotate the camera around the center of the scene.
  Use with *Alt* to enable first person camera behaviour.
  Use with *Strg/Ctrl* + *Alt* to rotate the camera around the center of the scene and subsequently snap the up direction.
)";

    ImGui::MarkdownConfig config;
    config.formatCallback = ui::markdownFormatCallback;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
    ImGui::Begin("Help");
    ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
    ImGui::End();
}

Context::UpdateResult Context::update()
{
    switch (mInternal->ScreenshotRequest) {
    case ScreenshotRequestMode::Framebuffer:
        mInternal->makeScreenshot();
        mInternal->ScreenshotRequest = ScreenshotRequestMode::Nothing;
        break;
    case ScreenshotRequestMode::Full:
        mInternal->makeFullScreenshot();
        mInternal->ScreenshotRequest = ScreenshotRequestMode::Nothing;
        break;
    default:
        break;
    }

    // The ImGui frame was begun in handleInput(); draw the docked render image and panels.
    const bool resized  = mInternal->drawRenderWindow();
    UpdateResult result = resized ? UpdateResult::Reset : UpdateResult::Continue;

    if (mInternal->isAnyWindowShown()) {
        if (mInternal->handleImgui() == UpdateResult::Reset)
            result = UpdateResult::Reset;
        if (mInternal->ShowHelp)
            handleHelp();
    }

    ui::renderFrame(mInternal->Window);
    return result;
}

void Context::setTravelSpeed(float v)
{
    mInternal->CurrentTravelSpeed = std::max(1e-5f, v);
}
} // namespace IG