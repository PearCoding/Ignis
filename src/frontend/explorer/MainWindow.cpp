#include "MainWindow.h"
#include "Logger.h"
#include "Runtime.h"
#include "UI.h"
#include "Widget.h"

namespace IG {
SDL_Window* sWindow     = nullptr;
SDL_Renderer* sRenderer = nullptr;

class MainWindowInternal {
private:
    std::shared_ptr<Runtime> mCurrentRuntime;

    SDL_Window* mWindow     = nullptr;
    SDL_Renderer* mRenderer = nullptr;

    std::vector<std::shared_ptr<Widget>> mChildren;

    std::function<void(const Path&)> mDropCallback;

public:
    MainWindowInternal(size_t width, size_t height, float dpi)
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

        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

        ui::setup(mWindow, mRenderer, true, dpi);

        sWindow   = mWindow;
        sRenderer = mRenderer;
    }

    ~MainWindowInternal()
    {
        ui::shutdown();

        if (mRenderer)
            SDL_DestroyRenderer(mRenderer);
        if (mWindow)
            SDL_DestroyWindow(mWindow);
        SDL_Quit();
    }

    inline void setTitle(const char* str)
    {
        SDL_SetWindowTitle(mWindow, str);
    }

    inline void setDropCallback(const std::function<void(const Path&)>& callback)
    {
        mDropCallback = callback;
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
            ui::newFrame();

            ImGui::ShowDemoWindow();

            for (const auto& child : mChildren)
                child->onRender(nullptr);

            ui::renderFrame();

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

        ui::notifyResize(mWindow, mRenderer);
    }

    bool handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui::processSDLEvent(event);

            switch (event.type) {
            case SDL_QUIT:
                return true;
            case SDL_DROPFILE: {
                Path path = event.drop.file;
                SDL_free(event.drop.file);

                if (mDropCallback)
                    mDropCallback(path);
            } break;
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    handleResize(event.window.data1, event.window.data2);
                    break;
                default:
                    break;
                }
            } break;
            default:
                break;
            }
        }

        for (const auto& child : mChildren)
            child->onInput(nullptr);

        return false;
    }
};

MainWindow::MainWindow(size_t width, size_t height, float dpi)
    : mInternal(std::make_unique<MainWindowInternal>(width, height, dpi))
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

void MainWindow::setDropCallback(const std::function<void(const Path&)>& callback)
{
    mInternal->setDropCallback(callback);
}
} // namespace IG