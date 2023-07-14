#include "UIGlue.h"

#include <filesystem>

#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#else
#define USE_OLD_SDL
// The following implementation is deprecated and only available for old SDL versions
#include "imgui_old_sdl.h"
#endif

namespace IGGui {
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

void processSDLEvent(const SDL_Event& event)
{
#ifndef USE_OLD_SDL
    ImGui_ImplSDL2_ProcessEvent(&event);
#else
    handleOldSDL(event);
    handleOldSDLMouse();
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

float getFontScale(SDL_Window* window, SDL_Renderer* renderer)
{
    (void)(renderer);

    int window_width{ 0 };
    int window_height{ 0 };
    SDL_GetWindowSize(
        window,
        &window_width, &window_height);

    int render_output_width{ 0 };
    int render_output_height{ 0 };

#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(window, &render_output_width, &render_output_height);
#else
    SDL_GetRendererOutputSize(
        renderer,
        &render_output_width, &render_output_height);
#endif

    if (window_width <= 0)
        return 1;

#if !defined(__WIN32__) || !defined(__APPLE__)
    if (render_output_width == window_width) {
        // In contrary to the warning, this is more reliable on Linux
        float dpi;
        SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), NULL, &dpi, NULL);
        return dpi / 96;
    }
#endif

    const auto scale_x{
        static_cast<float>(render_output_width) / static_cast<float>(window_width)
    };

    return scale_x;
}

static void setupStandardFont(SDL_Window* window, SDL_Renderer* renderer)
{
    auto& io = ImGui::GetIO();

#if defined(__WIN32__)
    const char* font_file = "C:\\Windows\\Fonts\\consola.ttf";
#elif defined(__APPLE__)
    const char* font_file = "/System/Library/Fonts/TODO.otf";
#else
    const char* font_file = "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf";
#endif

    const float font_scaling_factor = IGGui::getFontScale(window, renderer);
    if (!std::filesystem::exists(font_file)) {
        io.FontGlobalScale = font_scaling_factor;
    } else {
        ImFontConfig config;
        config.SizePixels    = 13 * font_scaling_factor;
        config.GlyphOffset.y = 1.0f * std::floor(config.SizePixels / 13.0f); // Add +1 offset per 13 units

        io.Fonts->AddFontFromFileTTF(font_file, config.SizePixels, &config);
    }
}

void setup(SDL_Window* window, SDL_Renderer* renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    IGGui::setupStandardFont(window, renderer);

#ifndef USE_OLD_SDL
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
#else
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    ImGuiSDL::Initialize(renderer, width, height);
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

    ImGui::DockSpaceOverViewport();
}

void renderFrame()
{
    ImGui::Render();
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
#else
    ImGuiSDL::Render(ImGui::GetDrawData());
#endif
}
} // namespace IGGui