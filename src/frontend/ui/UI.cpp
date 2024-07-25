#include "UI.h"
#include "Logger.h"
#include "RuntimeInfo.h"

#include <filesystem>

#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#else
#define USE_OLD_SDL
// The following implementation is deprecated and only available for old SDL versions
#include "imgui_old_sdl.h"
#endif

namespace IG::ui {
#ifdef USE_OLD_SDL
static void handleOldSDL(const SDL_Event& event)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (event.type) {
    case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event.text.text);
        break;
    case SDL_KEYUP:
    case SDL_KEYDOWN: {
        int key = event.key.keysym.scancode;
        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[key] = (event.type == SDL_KEYDOWN);
        io.KeyShift      = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl       = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt        = ((SDL_GetModState() & KMOD_ALT) != 0);
#ifdef _WIN32
        io.KeySuper = false;
#else
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
#endif
    } break;
    case SDL_MOUSEWHEEL:
        if (event.wheel.x > 0)
            io.MouseWheelH += 1;
        if (event.wheel.x < 0)
            io.MouseWheelH -= 1;
        if (event.wheel.y > 0)
            io.MouseWheel += 1;
        if (event.wheel.y < 0)
            io.MouseWheel -= 1;
        break;
    default:
        break;
    }
}

static void handleOldSDLMouse()
{
    ImGuiIO& io = ImGui::GetIO();

    int mouseX, mouseY;
    const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

    // Setup low-level inputs (e.g. on Win32, GetKeyboardState(), or write to those fields from your Windows message loop handlers, etc.)
    io.DeltaTime    = 1.0f / 60.0f;
    io.MousePos     = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
    io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
    io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
}
#endif

bool processSDLEvent(const SDL_Event& event)
{
#ifndef USE_OLD_SDL
    return ImGui_ImplSDL2_ProcessEvent(&event);
#else
    handleOldSDL(event);
    handleOldSDLMouse();
    return false;
#endif
}

void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_)
{
    switch (markdownFormatInfo_.type) {
    default:
        ImGui::defaultMarkdownFormatCallback(markdownFormatInfo_, start_);
        break;
    case ImGui::MarkdownFormatType::EMPHASIS:
        if (start_)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.16f, 0.57f, 0.94f, 1));
        else
            ImGui::PopStyleColor();
        break;
    }
}

static float sDPI = -1;
float getFontScale(SDL_Window* window, SDL_Renderer* renderer)
{
    if (sDPI > 0)
        return sDPI;

    int window_width  = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);

    int render_output_width  = 0;
    int render_output_height = 0;

#if SDL_VERSION_ATLEAST(2, 26, 0)
    IG_UNUSED(renderer);
    SDL_GetWindowSizeInPixels(window, &render_output_width, &render_output_height);
#else
    SDL_GetRendererOutputSize(renderer, &render_output_width, &render_output_height);
#endif

    if (window_width <= 0)
        return 1;

#if !defined(IG_OS_WINDOWS) && !defined(IG_OS_APPLE)
    if (render_output_width == window_width) {
        // In contrary to the warning, this is more reliable on Linux
        float dpi;
        SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), nullptr, &dpi, nullptr);
        sDPI = dpi / 96;
        IG_LOG(L_DEBUG) << "Detected DPI=" << sDPI << std::endl;
        return sDPI;
    }
#endif

    const float scale_x = (float)render_output_width / (float)window_width;

    sDPI = scale_x;
    IG_LOG(L_DEBUG) << "Detected DPI=" << sDPI << std::endl;
    return scale_x;
}

static void setupStandardFont(SDL_Window* window, SDL_Renderer* renderer)
{
    const auto dataPath = RuntimeInfo::readonlyDataPath();
    Path fontFile;
    if (!dataPath.empty())
        fontFile = dataPath / "fonts" / "UbuntuMono-R.ttf";

    if (!std::filesystem::exists(fontFile)) {
        IG_LOG(L_WARNING) << "Could not load custom font. Trying to use system default." << std::endl;
#if defined(IG_OS_WINDOWS)
        fontFile = "C:\\Windows\\Fonts\\consola.ttf";
#elif defined(IG_OS_APPLE)
        fontFile = "/System/Library/Fonts/TODO.otf";
#else
        fontFile = "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf";
#endif
    }

    auto& io = ImGui::GetIO();

    const float font_scaling_factor = getFontScale(window, renderer);
    if (!std::filesystem::exists(fontFile)) {
        io.FontGlobalScale = font_scaling_factor;
    } else {
        constexpr int DefaultFontSize = 13;
        ImFontConfig config;
        config.SizePixels    = DefaultFontSize * font_scaling_factor;
        config.PixelSnapH    = true;
        config.GlyphOffset.y = 1.0f * std::floor(config.SizePixels / (float)DefaultFontSize); // Add +1 offset per 13 units

        io.Fonts->AddFontFromFileTTF(fontFile.generic_string().c_str(), config.SizePixels, &config);
#ifdef IG_OS_WINDOWS
        // Why is this needed on Windows but not on other systems??
        io.FontGlobalScale = 1 / font_scaling_factor;
#endif
    }
}

void setup(SDL_Window* window, SDL_Renderer* renderer, bool useDocking, float dpi)
{
    sDPI = dpi;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifdef IMGUI_HAS_DOCK
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    if (useDocking)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#else
    IG_UNUSED(useDocking);
#endif

    setupStandardFont(window, renderer);

    ImGuiStyle& style  = ImGui::GetStyle();
    style.GrabRounding = 3;
    style.TabRounding  = 3;

#ifndef IG_OS_WINDOWS
    // Windows handles scaling different than other systems
    const float scale = getFontScale(window, renderer);
    style.ScaleAllSizes(scale);

    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        SDL_SetWindowSize(window, (int)(width * scale), (int)(height * scale));
    }
#endif

#ifndef USE_OLD_SDL
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
#else
    {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        ImGuiSDL::Initialize(renderer, width, height);
    }
#endif
}

void shutdown()
{
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
#else
    ImGuiSDL::Deinitialize();
#endif

    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void notifyResize(SDL_Window* window, SDL_Renderer* renderer)
{
    (void)window;
    (void)renderer;

#ifdef USE_OLD_SDL
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    ImGuiSDL::Initialize(renderer, width, height);
#endif
}

void newFrame()
{
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
#endif
    ImGui::NewFrame();
}

void renderFrame(SDL_Renderer* renderer)
{
    ImGui::Render();
    const auto& io = ImGui::GetIO();
    float prevScaleX, prevScaleY;
    SDL_RenderGetScale(renderer, &prevScaleX, &prevScaleY);
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
#else
    ImGuiSDL::Render(ImGui::GetDrawData());
#endif

    SDL_RenderSetScale(renderer, prevScaleX, prevScaleY);
}
} // namespace IG::ui