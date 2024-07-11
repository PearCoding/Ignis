#include "MainWindow.h"
#include "Logger.h"
#include "Runtime.h"
#include "UI.h"
#include "Widget.h"

namespace IG {
SDL_Window* sWindow     = nullptr;
SDL_Renderer* sRenderer = nullptr;
int sMainWindowDockID   = -1;

class MainWindowInternal {
private:
    std::shared_ptr<Runtime> mCurrentRuntime;

    SDL_Window* mWindow     = nullptr;
    SDL_Renderer* mRenderer = nullptr;

    std::vector<std::shared_ptr<Widget>> mChildren;

    std::function<void(const Path&)> mDropCallback;

    bool mQuit = false;

public:
    MainWindowInternal(size_t width, size_t height, float dpi)
    {
#ifdef IG_OS_WINDOWS
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            IG_LOG(L_FATAL) << "Cannot initialize SDL: " << SDL_GetError() << std::endl;
            throw std::runtime_error("Could not setup UI");
        }

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
        mQuit = false;

        // Initialize via resize
        int w, h;
        SDL_GetWindowSize(mWindow, &w, &h);

        for (const auto& child : mChildren)
            child->onWindowResize(nullptr, (size_t)w, (size_t)h);

        // Run the loop
        while (!mQuit) {
            const bool shouldQuit = handleEvents();
            if (shouldQuit)
                break;

            SDL_RenderClear(mRenderer);
            ui::newFrame();

            sMainWindowDockID = ImGui::DockSpaceOverViewport();
            // ImGui::ShowDemoWindow();

            for (const auto& child : mChildren)
                child->onRender(nullptr);

            ui::renderFrame(mRenderer);

            SDL_RenderPresent(mRenderer);
        }
        return true;
    }

    void addChild(const std::shared_ptr<Widget>& widget)
    {
        mChildren.emplace_back(widget);
    }

    void signalQuit()
    {
        mQuit = true;
    }

private:
    void handleResize(size_t width, size_t height)
    {
        for (const auto& child : mChildren)
            child->onWindowResize(nullptr, width, height);

        ui::notifyResize(mWindow, mRenderer);
    }

    bool handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (ui::processSDLEvent(event))
                continue; // Already handled

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

void MainWindow::setTitle(const std::string& str)
{
    mInternal->setTitle(str.c_str());
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

void MainWindow::signalQuit()
{
    mInternal->signalQuit();
}
} // namespace IG