#include "MainWindow.h"
#include "Logger.h"
#include "Runtime.h"
#include "Widget.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_markdown.h"
#include "implot.h"

#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#else
#define USE_OLD_SDL
// The following implementation is deprecated and only available for old SDL versions
#include "imgui_old_sdl.h"
#endif

namespace IG {
SDL_Window* sWindow     = nullptr;
SDL_Renderer* sRenderer = nullptr;

class MainWindowInternal {
private:
    std::shared_ptr<Runtime> mCurrentRuntime;

    SDL_Window* mWindow     = nullptr;
    SDL_Renderer* mRenderer = nullptr;

    std::vector<std::shared_ptr<Widget>> mChildren;

public:
    MainWindowInternal(size_t width, size_t height)
    {
        mWindow = SDL_CreateWindow(
            "Ignis",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            (int)width,
            (int)height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

        if (!mWindow) {
            IG_LOG(L_FATAL) << "Cannot create SDL window: " << SDL_GetError() << std::endl;
            throw std::runtime_error("Could not setup UI");
        }
        SDL_SetWindowMinimumSize(mWindow, 64, 64);

        mRenderer = SDL_CreateRenderer(mWindow, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (!mRenderer) {
            IG_LOG(L_FATAL) << "Cannot create SDL renderer: " << SDL_GetError() << std::endl;
            throw std::runtime_error("Could not setup UI");
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        (void)io;
#ifndef USE_OLD_SDL
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplSDL2_InitForSDLRenderer(mWindow, mRenderer);
        ImGui_ImplSDLRenderer2_Init(mRenderer);
#else
        ImGuiSDL::Initialize(mRenderer, (int)mWidth, (int)mHeight);
#endif
        sWindow   = mWindow;
        sRenderer = mRenderer;
    }

    ~MainWindowInternal()
    {
#ifndef USE_OLD_SDL
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
#else
        ImGuiSDL::Deinitialize();
#endif

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        if (mRenderer)
            SDL_DestroyRenderer(mRenderer);
        if (mWindow)
            SDL_DestroyWindow(mWindow);
        SDL_Quit();
    }

    void setTitle(const char* str)
    {
        SDL_SetWindowTitle(mWindow, str);
    }

    bool exec()
    {
        // Initialize via resize
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);

        for (const auto& child : mChildren)
            child->onResize(nullptr, (size_t)w, (size_t)h);

        // Run the loop
        while (true) {
            const bool shouldQuit = handleEvents();
            if (shouldQuit)
                break;

            SDL_RenderClear(mRenderer);

#ifndef USE_OLD_SDL
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
#endif
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            for (const auto& child : mChildren)
                child->onRender(nullptr);

            ImGui::Render();
#ifndef USE_OLD_SDL
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
#else
            ImGuiSDL::Render(ImGui::GetDrawData());
#endif

            SDL_RenderPresent(mRenderer);
        }
        return true;
    }

    void addChild(const std::shared_ptr<Widget>& widget)
    {
        mChildren.emplace_back(widget);
    }

private:
    void handleResize(size_t width, size_t height)
    {
        for (const auto& child : mChildren)
            child->onResize(nullptr, width, height);

#ifdef USE_OLD_SDL
        ImGuiSDL::Deinitialize();
        ImGuiSDL::Initialize(mRenderer, width, height);
#endif
    }

#ifdef USE_OLD_SDL
    void handleOldSDL(const SDL_Event& event)
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

    void handleOldSDLMouse()
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

    bool handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#ifndef USE_OLD_SDL
            ImGui_ImplSDL2_ProcessEvent(&event);
#else
            handleOldSDL(event);
#endif

            switch (event.type) {
            case SDL_QUIT:
                return true;
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    handleResize(event.window.data1, event.window.data2);
                    break;
                default:
                    break;
                }
            }
            default:
                break;
            }
        }

#ifdef USE_OLD_SDL
        handleOldSDLMouse();
#endif

        for (const auto& child : mChildren)
            child->onInput(nullptr);

        return false;
    }
};

MainWindow::MainWindow(size_t width, size_t height)
    : mInternal(std::make_unique<MainWindowInternal>(width, height))
{
}

MainWindow::~MainWindow()
{
}

bool MainWindow::exec()
{
    return mInternal->exec();
}

void MainWindow::addChild(const std::shared_ptr<Widget>& widget)
{
    mInternal->addChild(widget);
}
} // namespace IG