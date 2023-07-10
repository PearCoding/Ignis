#include "UI.h"

#include "UIGlue.h"

#include "Color.h"
#include "Logger.h"
#include "statistics/Statistics.h"

#include <algorithm>

namespace IG {

class UIInternal {
public:
    Statistics Stats;
    float TotalMS = 0;

    UI* Parent             = nullptr;
    SDL_Window* Window     = nullptr;
    SDL_Renderer* Renderer = nullptr;

    bool ShowHelp       = false;
    bool ShowMeans      = true;
    bool ShowQuantities = true;
    bool ShowChart      = true;

    void handleFramebufferResize(int width, int height)
    {
        IG_UNUSED(width);
        IG_UNUSED(height);

#ifdef USE_OLD_SDL
        ImGuiSDL::Deinitialize();
        ImGuiSDL::Initialize(Renderer, width, height);
#endif
    }

    // Events
    UI::InputResult handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            IGGui::processSDLEventForImGUI(event);

            // First handle ImGui stuff
            switch (event.type) {
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return UI::InputResult::Quit;
                case SDLK_F1:
                    ShowHelp = !ShowHelp;
                    break;
                case SDLK_F2:
                    ShowChart = !ShowChart;
                    break;
                case SDLK_F3:
                    ShowQuantities = !ShowQuantities;
                    break;
                case SDLK_F4:
                    ShowMeans = !ShowMeans;
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
                    break;
                default:
                    break;
                }
            }
            default:
                break;
            }
        }

        return UI::InputResult::Continue;
    }

    static constexpr int WindowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;

    void handleQuantityWindow()
    {
        const size_t totalIter = Stats.entry(ShaderType::Device).count;

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Once);
        if (ImGui::Begin("Quantities")) {
            if (ImGui::BeginTable("table_quantity", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Total");
                ImGui::TableSetupColumn("Per Iteration");
                ImGui::TableSetupColumn("Per Second");
                ImGui::TableHeadersRow();

                uint64 camera = Stats.quantity(Quantity::CameraRayCount);
                uint64 bounce = Stats.quantity(Quantity::BounceRayCount);
                uint64 shadow = Stats.quantity(Quantity::ShadowRayCount);

                std::array<std::tuple<const char*, uint64>, 6> values = {
                    std::make_tuple("Iteration", (uint64)totalIter),
                    std::make_tuple("Camera Ray Count", camera),
                    std::make_tuple("Bounce Ray Count", bounce),
                    std::make_tuple("Shadow Ray Count", shadow),
                    std::make_tuple("Primary Ray Count", camera + bounce),
                    std::make_tuple("Total Ray Count", camera + bounce + shadow)
                };

                for (size_t i = 0; i < values.size(); ++i) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(std::get<0>(values[i]));

                    uint64 total = std::get<1>(values[i]);
                    ImGui::TableNextColumn();
                    ImGui::Text("%" PRIu64, total);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", static_cast<float>(total) / totalIter);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", 1000 * static_cast<float>(total) / TotalMS);
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void handleMeansWindow()
    {
    }

    void handleChartWindow()
    {
    }

    static void handleHelpWindow()
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
        config.formatCallback = IGGui::markdownFormatCallback;

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
        if (ImGui::Begin("Help")) {
            ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
        }
        ImGui::End();
    }

    void handleImgui()
    {
        ImGui::ShowDemoWindow();

        if (ShowQuantities)
            handleQuantityWindow();

        if (ShowMeans)
            handleMeansWindow();

        if (ShowChart)
            handleChartWindow();

        if (ShowHelp)
            handleHelpWindow();
    }
};

////////////////////////////////////////////////////////////////
static float get_scale(SDL_Window* window, SDL_Renderer* renderer)
{
    int window_width{ 0 };
    int window_height{ 0 };
    SDL_GetWindowSize(
        window,
        &window_width, &window_height);

    int render_output_width{ 0 };
    int render_output_height{ 0 };
    SDL_GetRendererOutputSize(
        renderer,
        &render_output_width, &render_output_height);

    const auto scale_x{
        static_cast<float>(render_output_width) / static_cast<float>(window_width)
    };

    return scale_x;
}

UI::UI(const Statistics& stats, float total_ms)
    : mInternal(std::make_unique<UIInternal>())
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        IG_LOG(L_FATAL) << "Cannot initialize SDL: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Could not setup UI");
    }

    mInternal->Stats   = stats;
    mInternal->TotalMS = total_ms;
    mInternal->Parent  = this;

    mInternal->Window = SDL_CreateWindow(
        "Ignis Profiler",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800,
        600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    const float font_scaling_factor = get_scale(mInternal->Window, mInternal->Renderer);
    io.FontGlobalScale              = 1.0F / font_scaling_factor;

#ifndef USE_OLD_SDL
    ImGui_ImplSDL2_InitForSDLRenderer(mInternal->Window, mInternal->Renderer);
    ImGui_ImplSDLRenderer2_Init(mInternal->Renderer);
#else
    int width, height;
    SDL_GetWindowSize(mInternal->Window, &width, &height);
    ImGuiSDL::Initialize(mInternal->Renderer, width, height);
#endif
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

    if (mInternal->Renderer)
        SDL_DestroyRenderer(mInternal->Renderer);
    if (mInternal->Window)
        SDL_DestroyWindow(mInternal->Window);
    SDL_Quit();
}

UI::InputResult UI::handleInput()
{
    return mInternal->handleEvents();
}

void UI::update()
{
    SDL_RenderClear(mInternal->Renderer);

#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
#endif
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport();

    mInternal->handleImgui();

    ImGui::Render();
#ifndef USE_OLD_SDL
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
#else
    ImGuiSDL::Render(ImGui::GetDrawData());
#endif

    SDL_RenderPresent(mInternal->Renderer);
}
} // namespace IG