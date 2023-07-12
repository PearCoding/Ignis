#include "UI.h"
#include "IO.h"
#include "Pose.h"
#include "Runtime.h"

#include "UIGlue.h"

#include "Inspector.h"
#include "PropertyView.h"

#include "Color.h"
#include "Logger.h"

#include <algorithm>

namespace IG {

constexpr size_t HISTOGRAM_SIZE                     = 50;
static const char* const ToneMappingMethodOptions[] = {
    "None", "Reinhard", "Mod. Reinhard", "ACES", "Uncharted2"
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

class UIInternal {
public:
    IG::Runtime* Runtime   = nullptr;
    UI* Parent             = nullptr;
    SDL_Window* Window     = nullptr;
    SDL_Renderer* Renderer = nullptr;
    SDL_Texture* Texture   = nullptr;
    std::vector<uint32_t> Buffer;

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
    float ToneMapping_Exposure              = 1.0f;
    float ToneMapping_Offset                = 0.0f;
    bool ToneMappingGamma                   = true;
    IG::ToneMappingMethod ToneMappingMethod = ToneMappingMethod::ACES;

    // Visualization
    bool VisualizeGlare             = true;
    float VisualizeGlare_Multiplier = 5.0f;

    bool VisualizeGlare_AutoEV = true;
    float VerticalIlluminance  = 5.0f;
    GlareOutput Glare;

    size_t CurrentAOV = 0;

    bool Running       = true;
    bool ShowDebugMode = false;

    IG::PoseManager PoseManager;
    CameraPose LastCameraPose;

    float CurrentTravelSpeed = 1.0f;
    float CurrentZoom        = 1.0f; // Only important if orthogonal
    float DefaultCameraScale = 1.0f;

    inline bool isAnyWindowShown() const { return ShowControl || ShowProperties || ShowInspector || ShowHelp; }

    // Buffer stuff
    bool setupTextureBuffer(size_t width, size_t height)
    {
        if (Texture)
            SDL_DestroyTexture(Texture);

        Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
        if (!Texture) {
            IG_LOG(L_FATAL) << "Cannot create SDL texture: " << SDL_GetError() << std::endl;
            return false;
        }

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

    void handleFramebufferResize(int width, int height)
    {
        // Expect a useful minimum!
        if (width <= 5 || height <= 5)
            return;

        IG_LOG(L_INFO) << "Resizing to " << width << "x" << height << std::endl;

        Runtime->resizeFramebuffer((size_t)width, (size_t)height);
        Width  = width;
        Height = height;
        setupTextureBuffer((size_t)width, (size_t)height);

#ifdef USE_OLD_SDL
        ImGuiSDL::Deinitialize();
        ImGuiSDL::Initialize(Renderer, width, height);
#endif
    }

    [[nodiscard]] inline std::string currentAOVName() const
    {
        if (CurrentAOV == 0)
            return std::string{};
        else
            return Runtime->aovs().at(CurrentAOV - 1);
    }

    [[nodiscard]] inline AOVAccessor currentPixels() const
    {
        return Runtime->getFramebufferForHost(currentAOVName());
    }

    void changeAOV(int delta_aov)
    {
        const int rem = (int)Runtime->aovs().size() + 1;
        CurrentAOV    = static_cast<size_t>((((int)CurrentAOV + delta_aov) % rem + rem) % rem);
    }

    enum MouseMode {
        MM_None,
        MM_Look,
        MM_Pan
    };

    // Events
    UI::InputResult handleEvents(CameraProxy& cam)
    {
        const Vector3f sceneCenter = Runtime->sceneBoundingBox().center();

        static bool first_call = true;
        if (first_call) {
            PoseManager.setInitalPose(CameraPose(cam));
            first_call = false;
        }

        ImGuiIO& io = ImGui::GetIO();

        static MouseMode mouse_mode        = MM_None;
        static std::array<bool, 12> arrows = { false, false, false, false, false, false, false, false, false, false, false, false };
        static bool speed[2]               = { false, false };
        constexpr float RSPEED             = 0.005f;

        const bool canInteract = !LockInteraction && Running;

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
        SDL_Event event;
        const bool hover = isAnyWindowShown() && (ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow));
        while (SDL_PollEvent(&event)) {
            IGGui::processSDLEventForImGUI(event);

            // First handle ImGui stuff
            bool key_down = event.type == SDL_KEYDOWN;
            switch (event.type) {
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return UI::InputResult::Quit;
                case SDLK_t:
                    if (!io.WantTextInput)
                        ToneMapping_Automatic = !ToneMapping_Automatic;
                    break;
                case SDLK_g:
                    if (!ToneMapping_Automatic && !io.WantTextInput) {
                        ToneMapping_Exposure = 1.0f;
                        ToneMapping_Offset   = 0.0f;
                    }
                    break;
                case SDLK_f:
                    if (!ToneMapping_Automatic && !io.WantTextInput) {
                        const float delta = io.KeyCtrl ? 0.05f : 0.5f;
                        ToneMapping_Exposure += io.KeyShift ? -delta : delta;
                    }
                    break;
                case SDLK_v:
                    if (!ToneMapping_Automatic && !io.WantTextInput) {
                        const float delta = io.KeyCtrl ? 0.05f : 0.5f;
                        ToneMapping_Offset += io.KeyShift ? -delta : delta;
                    }
                    break;
                case SDLK_p:
                    if (!io.WantTextInput) {
                        if (Running) {
                            Running = false;
                            return UI::InputResult::Pause;
                        } else {
                            Running = true;
                            return UI::InputResult::Resume;
                        }
                    }
                    break;
                case SDLK_n:
                    if (!io.WantTextInput)
                        changeAOV(-1);
                    break;
                case SDLK_m:
                    if (!io.WantTextInput)
                        changeAOV(1);
                    break;
                case SDLK_i:
                    if (!io.WantTextInput)
                        ShowInspector = !ShowInspector;
                    break;
                case SDLK_F1:
                    ShowHelp = !ShowHelp;
                    break;
                case SDLK_F2:
                    ShowControl = !ShowControl;
                    break;
                case SDLK_F3:
                    LockInteraction = !LockInteraction;
                    break;
                case SDLK_F4:
                    ShowProperties = !ShowProperties;
                    break;
                case SDLK_F11:
                    if (io.KeyCtrl)
                        ScreenshotRequest = ScreenshotRequestMode::Full;
                    else
                        ScreenshotRequest = ScreenshotRequestMode::Framebuffer;
                    break;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                return UI::InputResult::Quit;
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    handleFramebufferResize(event.window.data1, event.window.data2);
                    reset = true;
                    break;
                default:
                    break;
                }
            }
            default:
                break;
            }

            // Skip application input if any ImGui window is captured
            if (isAnyWindowShown() && (io.WantCaptureKeyboard || io.WantCaptureMouse || io.WantTextInput))
                continue;

            // Handle application input
            switch (event.type) {
            case SDL_KEYUP:
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_KP_PLUS:
                    speed[0] = key_down;
                    break;
                case SDLK_KP_MINUS:
                    speed[1] = key_down;
                    break;
                case SDLK_UP:
                    arrows[0] = key_down;
                    break;
                case SDLK_w:
                    arrows[0] = key_down;
                    break;
                case SDLK_DOWN:
                    arrows[1] = key_down;
                    break;
                case SDLK_s:
                    arrows[1] = key_down;
                    break;
                case SDLK_LEFT:
                    arrows[2] = key_down;
                    break;
                case SDLK_a:
                    arrows[2] = key_down;
                    break;
                case SDLK_RIGHT:
                    arrows[3] = key_down;
                    break;
                case SDLK_d:
                    arrows[3] = key_down;
                    break;
                case SDLK_e:
                    arrows[4] = key_down;
                    break;
                case SDLK_q:
                    arrows[5] = key_down;
                    break;
                case SDLK_PAGEUP:
                    arrows[6] = key_down;
                    break;
                case SDLK_PAGEDOWN:
                    arrows[7] = key_down;
                    break;
                case SDLK_KP_2:
                    arrows[8] = key_down;
                    break;
                case SDLK_KP_8:
                    arrows[9] = key_down;
                    break;
                case SDLK_KP_4:
                    arrows[10] = key_down;
                    break;
                case SDLK_KP_6:
                    arrows[11] = key_down;
                    break;
                }

                // Followings should only be handled once
                if (event.type == SDL_KEYUP) {
                    const bool capture = io.KeyCtrl;
                    if (canInteract) {
                        switch (event.key.keysym.sym) {
                        case SDLK_KP_1:
                            cam.update_dir(Vector3f(0, 0, 1), Vector3f(0, 1, 0));
                            reset = true;
                            break;
                        case SDLK_KP_3:
                            cam.update_dir(Vector3f(1, 0, 0), Vector3f(0, 1, 0));
                            reset = true;
                            break;
                        case SDLK_KP_7:
                            cam.update_dir(Vector3f(0, 1, 0), Vector3f(0, 0, 1));
                            reset = true;
                            break;
                        case SDLK_KP_9:
                            cam.update_dir(-cam.Direction, cam.Up);
                            reset = true;
                            break;
                        case SDLK_1:
                            handlePoseInput(0, capture, cam);
                            break;
                        case SDLK_2:
                            handlePoseInput(1, capture, cam);
                            break;
                        case SDLK_3:
                            handlePoseInput(2, capture, cam);
                            break;
                        case SDLK_4:
                            handlePoseInput(3, capture, cam);
                            break;
                        case SDLK_5:
                            handlePoseInput(4, capture, cam);
                            break;
                        case SDLK_6:
                            handlePoseInput(5, capture, cam);
                            break;
                        case SDLK_7:
                            handlePoseInput(6, capture, cam);
                            break;
                        case SDLK_8:
                            handlePoseInput(7, capture, cam);
                            break;
                        case SDLK_9:
                            handlePoseInput(8, capture, cam);
                            break;
                        case SDLK_0:
                            handlePoseInput(9, capture, cam);
                            break;
                        case SDLK_o:
                            cam.snap_up();
                            reset = true;
                            break;
                        case SDLK_r:
                            PoseResetRequest = true;
                            break;
                        }
                    }
                }
            } break;
            case SDL_MOUSEBUTTONDOWN:
                if (!hover && canInteract) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
#ifndef IG_DEBUG
                        SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
                        mouse_mode = MM_Look;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
#ifndef IG_DEBUG
                        SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
                        mouse_mode = MM_Pan;
                    }
                }
                break;
            case SDL_MOUSEBUTTONUP:
#ifndef IG_DEBUG
                SDL_SetRelativeMouseMode(SDL_FALSE);
#endif
                mouse_mode = MM_None;
                break;
            case SDL_MOUSEMOTION:
                if (!hover && canInteract) {
                    switch (mouse_mode) {
                    default:
                    case MM_None:
                        break;
                    case MM_Look: {
                        const float aspeed  = RSPEED;
                        const float xmotion = event.motion.xrel * aspeed;
                        const float ymotion = event.motion.yrel * aspeed;
                        handleRotation(xmotion, ymotion);
                        reset = true;
                    } break;
                    case MM_Pan: {
                        const float aspeed  = CurrentTravelSpeed / 10;
                        const float xmotion = event.motion.xrel * aspeed;
                        const float ymotion = -event.motion.yrel * aspeed;
                        cam.move(xmotion, ymotion, 0);
                        reset = true;
                    } break;
                    }
                }
                break;
            case SDL_MOUSEWHEEL:
                if (!hover && canInteract) {
                    if (event.wheel.y != 0) {
                        if (ZoomIsScale)
                            CurrentZoom *= (event.wheel.y < 0) ? -event.wheel.y * 1.05f : event.wheel.y * 0.95f;
                        else
                            cam.move(0, 0, event.wheel.y * CurrentTravelSpeed);
                        reset = true;
                    }
                }
                break;
            default:
                break;
            }
        }

#ifdef USE_OLD_SDL
        handleOldSDLMouse();
#endif

        if (canInteract) {
            if (std::any_of(arrows.begin(), arrows.end(), [](bool b) { return b; }))
                reset = true;

            constexpr float KRSPEED = 10 * RSPEED;
            if (arrows[0])
                cam.move(0, 0, CurrentTravelSpeed);
            if (arrows[1])
                cam.move(0, 0, -CurrentTravelSpeed);
            if (arrows[2])
                cam.move(-CurrentTravelSpeed, 0, 0);
            if (arrows[3])
                cam.move(CurrentTravelSpeed, 0, 0);
            if (arrows[4])
                cam.roll(KRSPEED);
            if (arrows[5])
                cam.roll(-KRSPEED);
            if (arrows[6])
                cam.move(0, CurrentTravelSpeed, 0);
            if (arrows[7])
                cam.move(0, -CurrentTravelSpeed, 0);
            if (arrows[8])
                handleRotation(0, KRSPEED);
            if (arrows[9])
                handleRotation(0, -KRSPEED);
            if (arrows[10])
                handleRotation(-KRSPEED, 0);
            if (arrows[11])
                handleRotation(KRSPEED, 0);
            if (speed[0])
                CurrentTravelSpeed *= 1.1f;
            if (speed[1])
                CurrentTravelSpeed *= 0.9f;

            if (PoseResetRequest || PoseRequest >= 0) {
                auto pose = PoseResetRequest ? PoseManager.initialPose() : PoseManager.pose(PoseRequest);
                cam.Eye   = pose.Eye;
                cam.update_dir(pose.Dir, pose.Up);
                CurrentZoom      = 1;
                PoseRequest      = -1;
                PoseResetRequest = false;

                reset = true;
            }
        }

        LastCameraPose = CameraPose(cam);
        if (Running && ZoomIsScale)
            Runtime->setParameter("__camera_scale", DefaultCameraScale * CurrentZoom);

        return reset ? UI::InputResult::Reset : UI::InputResult::Continue;
    }

    void analyzeLuminance()
    {
        const std::string aov_name = currentAOVName();
        ImageInfoSettings settings{ aov_name.c_str(),
                                    1.0f, HISTOGRAM_SIZE,
                                    Histogram.data() + 0 * HISTOGRAM_SIZE, Histogram.data() + 1 * HISTOGRAM_SIZE,
                                    Histogram.data() + 2 * HISTOGRAM_SIZE, Histogram.data() + 3 * HISTOGRAM_SIZE,
                                    true, true };

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
        const std::string aov_name = currentAOVName();
        analyzeLuminance();

        // TODO: It should be possible to directly change the device buffer (if the computing device is the display device)... but thats very advanced
        uint32* buf = Buffer.data();
        Runtime->tonemap(buf, TonemapSettings{ aov_name.c_str(), (size_t)ToneMappingMethod, ToneMappingGamma,
                                               1.0f,
                                               ToneMapping_Automatic ? 1 / LastLum.Est : std::pow(2.0f, ToneMapping_Exposure),
                                               ToneMapping_Automatic ? 0 : ToneMapping_Offset });

        if (Runtime->options().Glare.Enabled && VisualizeGlare)
            Glare = Runtime->evaluateGlare(buf, GlareSettings{ aov_name.c_str(), 1.0f, LastLum.SoftMax, LastLum.Avg, VisualizeGlare_Multiplier, VisualizeGlare_AutoEV ? -1.0f : VerticalIlluminance });

        SDL_UpdateTexture(Texture, nullptr, buf, static_cast<int>(Width * sizeof(uint32_t)));
    }

    [[nodiscard]] inline RGB getFilmData(size_t width, size_t height, uint32_t x, uint32_t y)
    {
        IG_UNUSED(height);

        const auto acc       = currentPixels();
        const float* film    = acc.Data;
        const float inv_iter = acc.IterationCount > 0 ? 1.0f / acc.IterationCount : 0.0f;
        const size_t ind     = y * width + x;

        return RGB{
            film[ind * 3 + 0] * inv_iter,
            film[ind * 3 + 1] * inv_iter,
            film[ind * 3 + 2] * inv_iter
        };
    }

    void makeScreenshot()
    {
        std::stringstream out_file;
        auto now       = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        out_file << "screenshot_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

        CameraOrientation orientation;
        orientation.Eye = LastCameraPose.Eye;
        orientation.Up  = LastCameraPose.Up;
        orientation.Dir = LastCameraPose.Dir;
        if (!saveImageOutput(out_file.str(), *Runtime, &orientation))
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

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        Uint32 rmask = 0xff000000;
        Uint32 gmask = 0x00ff0000;
        Uint32 bmask = 0x0000ff00;
        Uint32 amask = 0x000000ff;
#else
        Uint32 rmask = 0x000000ff;
        Uint32 gmask = 0x0000ff00;
        Uint32 bmask = 0x00ff0000;
        Uint32 amask = 0xff000000;
#endif

        SDL_Surface* sshot = SDL_CreateRGBSurface(0, (int)Width, (int)Height, 32,
                                                  rmask, gmask, bmask, amask);

        if (!sshot) {
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
            return;
        }

        int ret = SDL_LockSurface(sshot);
        if (ret != 0) {
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
            return;
        }

        ret = SDL_RenderReadPixels(Renderer, nullptr, sshot->format->format,
                                   sshot->pixels, sshot->pitch);
        if (ret != 0) {
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
            return;
        }

        float* rgba = new float[Width * Height * 4];
        for (size_t y = 0; y < Height; ++y) {
            const uint8* src = reinterpret_cast<const uint8*>(sshot->pixels) + y * sshot->pitch;
            float* dst       = rgba + y * Width * 4;
            for (size_t x = 0; x < Width; ++x) {
                uint32 pixel = *reinterpret_cast<const uint32*>(&src[x * sshot->format->BytesPerPixel]);

                uint8 r, g, b, a;
                SDL_GetRGBA(pixel, sshot->format, &r, &g, &b, &a);

                // EXR is linear, so remove gamma part
                dst[x * 4 + 0] = std::pow(r / 255.0f, 2.2f);
                dst[x * 4 + 1] = std::pow(g / 255.0f, 2.2f);
                dst[x * 4 + 2] = std::pow(b / 255.0f, 2.2f);
                dst[x * 4 + 3] = a / 255.0f; // Do not map alpha channel (which should be 1 99% of the time)
            }
        }

        SDL_UnlockSurface(sshot);
        SDL_FreeSurface(sshot);

        if (!saveImageRGBA(out_file.str(), rgba, Width, Height, 1))
            IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
        else
            IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;

        delete[] rgba;
    }

    static constexpr int WindowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
    void handleControlWindow()
    {
        constexpr size_t UI_W   = 350;
        constexpr size_t UI_H   = 600;
        constexpr size_t HIST_W = 300;

        ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(UI_W, UI_H), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Control", &ShowControl, WindowFlags)) {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);
                RGB rgb{ 0, 0, 0 };
                if (mouse_x >= 0 && mouse_x < (int)Width && mouse_y >= 0 && mouse_y < (int)Height)
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
                    ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0), 0); // No lines
                    ImPlot::SetNextFillStyle(ImVec4(1, 0, 0, 1), 0.25f);
                    ImPlot::PlotBars("R", Histogram.data() + 0 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, ImPlotBarsFlags_None);
                    ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0), 0); // No lines
                    ImPlot::SetNextFillStyle(ImVec4(0, 1, 0, 1), 0.25f);
                    ImPlot::PlotBars("G", Histogram.data() + 1 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, ImPlotBarsFlags_None);
                    ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0), 0); // No lines
                    ImPlot::SetNextFillStyle(ImVec4(0, 0, 1, 1), 0.25f);
                    ImPlot::PlotBars("B", Histogram.data() + 2 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, ImPlotBarsFlags_None);
                    ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0), 0); // No lines
                    ImPlot::SetNextFillStyle(ImVec4(1, 1, 0, 1), 0.25f);
                    ImPlot::PlotBars("L", Histogram.data() + 3 * HISTOGRAM_SIZE, HISTOGRAM_SIZE, BarWidth, 0, ImPlotBarsFlags_None);

                    ImPlot::EndPlot();
                }
                ImPlot::PopStyleVar();
                ImGui::PopItemWidth();
            }

            if (!Runtime->aovs().empty()) {
                if (ImGui::CollapsingHeader("AOV", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const char* current_aov = CurrentAOV == 0 ? "Color" : Runtime->aovs().at(CurrentAOV - 1).c_str();
                    if (ImGui::BeginCombo("Display", current_aov)) {
                        for (size_t i = 0; i < Runtime->aovs().size() + 1; ++i) {
                            bool is_selected = (i == CurrentAOV);
                            const char* name = i == 0 ? "Color" : Runtime->aovs().at(i - 1).c_str();
                            if (ImGui::Selectable(name, is_selected))
                                CurrentAOV = (int)i;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }
                }
            }

            if (ShowDebugMode) {
                if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static auto debugModeNames = getDebugModeNames();
                    ImGui::BeginDisabled(!Running);
                    std::string current_method = debugModeToString(Parent->mDebugMode);
                    if (ImGui::BeginCombo("Mode", current_method.c_str())) {
                        for (const auto& s : debugModeNames) {
                            bool is_selected = (current_method == s);
                            if (ImGui::Selectable(s.c_str(), is_selected) && Running)
                                Parent->mDebugMode = stringToDebugMode(s).value();
                            if (is_selected && Running)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::EndDisabled();
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

            if (Runtime->options().Glare.Enabled) {
                if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Visualize Glare", &VisualizeGlare);
                    ImGui::Text("Avg. Luminance: %1.4f Lux", 179 * LastLum.Avg);
                    if (Glare.NumPixels > 0) {
                        ImGui::Text("Avg. GS Luminance: %1.4f Lux", Glare.AvgLum);
                        ImGui::Text("Avg. GS Omega: %1.4f", Glare.AvgOmega);
                        ImGui::Text("GS Pixel No: %i", Glare.NumPixels);
                    } else {
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No glare source detected!");
                    }
                    ImGui::SliderFloat("Multiplier", &VisualizeGlare_Multiplier, 0.0, 20.0);
                    ImGui::Checkbox("Automatic EV", &VisualizeGlare_AutoEV);
                    if (!VisualizeGlare_AutoEV)
                        ImGui::SliderFloat("EV", &VerticalIlluminance, 0.0, 500.0);
                    else
                        ImGui::Text("EV: %1.3f", Glare.VerticalIlluminance);
                    ImGui::Text("DGP: %1.3f", Glare.DGP);
                }
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

    UI::UpdateResult handlePropertyWindow()
    {
        constexpr size_t PROP_W = 350;
        constexpr size_t PROP_H = 400;
        UI::UpdateResult result = UI::UpdateResult::Continue;
        ImGui::SetNextWindowPos(ImVec2((float)(Width - 5 - PROP_W), 5.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)PROP_W, (float)PROP_H), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Properties", &ShowProperties, WindowFlags)) {
            const bool changed = ui_property_view(Runtime);
            if (changed)
                result = UI::UpdateResult::Reset;
        }
        ImGui::End();
        return result;
    }

    UI::UpdateResult handleImgui()
    {
        if (ShowControl)
            handleControlWindow();

        UI::UpdateResult result = UI::UpdateResult::Continue;
        if (ShowProperties)
            result = handlePropertyWindow();

        if (ShowInspector) {
            int mouse_x, mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            const auto acc = currentPixels();
            ui_inspect_image(mouse_x, mouse_y, Width, Height, acc.IterationCount == 0 ? 1.0f : 1.0f / acc.IterationCount, acc.Data, Buffer.data());
        }

        return result;
    }
};

////////////////////////////////////////////////////////////////

UI::UI(SPPMode sppmode, Runtime* runtime, bool showDebug)
    : mSPPMode(sppmode)
    , mDebugMode(DebugMode::Normal)
    , mInternal(std::make_unique<UIInternal>())
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        IG_LOG(L_FATAL) << "Cannot initialize SDL: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }

    mInternal->Runtime       = runtime;
    mInternal->Parent        = this;
    mInternal->Width         = runtime->framebufferWidth();
    mInternal->Height        = runtime->framebufferHeight();
    mInternal->ShowDebugMode = showDebug;
    mInternal->ZoomIsScale   = runtime->camera() == "orthogonal";

    if (auto it = runtime->parameters().FloatParameters.find("__camera_scale"); it != runtime->parameters().FloatParameters.end())
        mInternal->DefaultCameraScale = it->second;

    mInternal->Window = SDL_CreateWindow(
        "Ignis",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        (int)mInternal->Width,
        (int)mInternal->Height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!mInternal->Window) {
        IG_LOG(L_FATAL) << "Cannot create SDL window: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }
    SDL_SetWindowMinimumSize(mInternal->Window, 64, 64);

    mInternal->Renderer = SDL_CreateRenderer(mInternal->Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!mInternal->Renderer) {
        IG_LOG(L_FATAL) << "Cannot create SDL renderer: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }

    if (!mInternal->setupTextureBuffer(mInternal->Width, mInternal->Height))
        throw std::runtime_error("Could not setup UI");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    IGGui::setupStandardFont(mInternal->Window, mInternal->Renderer);

#ifndef USE_OLD_SDL
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts; // We do it manually

    ImGui_ImplSDL2_InitForSDLRenderer(mInternal->Window, mInternal->Renderer);
    ImGui_ImplSDLRenderer2_Init(mInternal->Renderer);
#else
    ImGuiSDL::Initialize(mInternal->Renderer, (int)mInternal->Width, (int)mInternal->Height);
#endif

    mInternal->PoseManager.load(POSE_FILE);

    mInternal->ShowProperties = runtime->hasSceneParameters();

    if (mInternal->Width < 350 || mInternal->Height < 500) {
        IG_LOG(L_WARNING) << "Window too small to show UI. Hiding it by default. Press F2 or F4 to show it" << std::endl;
        mInternal->ShowControl    = false;
        mInternal->ShowProperties = false;
    }
}

UI::~UI()
{
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
#else
    ImGuiSDL::Deinitialize();
#endif

    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (mInternal->Texture)
        SDL_DestroyTexture(mInternal->Texture);
    if (mInternal->Renderer)
        SDL_DestroyRenderer(mInternal->Renderer);
    if (mInternal->Window)
        SDL_DestroyWindow(mInternal->Window);
    SDL_Quit();

    mInternal->Buffer.clear();
}

void UI::setTitle(const char* str)
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
    SDL_SetWindowTitle(mInternal->Window, sstream.str().c_str());
}

UI::InputResult UI::handleInput(CameraProxy& cam)
{
    return mInternal->handleEvents(cam);
}

static void handleHelp()
{
    static const std::string Markdown =
        R"(- *F1* to toggle this help window.
- *F2* to toggle the quantity window.
- *F3* to toggle the means window.
- *F4* to toggle the chart window.
)";

    ImGui::MarkdownConfig config;
    config.formatCallback = IGGui::markdownFormatCallback;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_FirstUseEver);
    ImGui::Begin("Help");
    ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
    ImGui::End();
}

UI::UpdateResult UI::update()
{
    mInternal->updateSurface();
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
    SDL_RenderClear(mInternal->Renderer);
    SDL_RenderCopy(mInternal->Renderer, mInternal->Texture, nullptr, nullptr);

#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
#endif
    ImGui::NewFrame();

    UpdateResult result = UpdateResult::Continue;
    if (mInternal->isAnyWindowShown()) {
        result = mInternal->handleImgui();
        if (mInternal->ShowHelp)
            handleHelp();
    }

    ImGui::Render();
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
#else
    ImGuiSDL::Render(ImGui::GetDrawData());
#endif

    SDL_RenderPresent(mInternal->Renderer);
    return result;
}

void UI::setTravelSpeed(float v)
{
    mInternal->CurrentTravelSpeed = std::max(1e-5f, v);
}
} // namespace IG