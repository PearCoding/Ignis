#include "Application.h"
#include "Logger.h"
#include "Runtime.h"
#include "UI.h"
#include "Widget.h"

namespace IG {
int sMainWindowDockID = -1;

class ApplicationInternal {
private:
    GLFWwindow* mWindow = nullptr;

    std::vector<std::shared_ptr<Widget>> mChildren;

    std::function<void(const Path&)> mDropCallback;

    bool mQuit = false;

    // ImGui_ImplGlfw does not install a drop callback, so we own this one directly.
    static void dropCallback(GLFWwindow* window, int count, const char** paths)
    {
        auto* self = static_cast<ApplicationInternal*>(glfwGetWindowUserPointer(window));
        if (!self || !self->mDropCallback)
            return;

        for (int i = 0; i < count; ++i)
            self->mDropCallback(Path(paths[i]));
    }

public:
    ApplicationInternal(size_t width, size_t height, float dpi)
    {
        if (!ui::setup(mWindow, (int)width, (int)height, "Ignis", true, dpi)) {
            IG_LOG(L_FATAL) << "Could not setup UI" << std::endl;
            throw std::runtime_error("Could not setup UI");
        }

        glfwSetWindowUserPointer(mWindow, this);
        glfwSetDropCallback(mWindow, dropCallback);
    }

    ~ApplicationInternal()
    {
        // Widgets may own OpenGL resources, which must be released while the context is still current
        mChildren.clear();
        ui::shutdown(mWindow);
    }

    inline void setTitle(const char* str)
    {
        glfwSetWindowTitle(mWindow, str);
    }

    inline void setDropCallback(const std::function<void(const Path&)>& callback)
    {
        mDropCallback = callback;
    }

    bool exec()
    {
        mQuit = false;

        int lastWidth  = -1;
        int lastHeight = -1;

        // Run the loop
        while (!mQuit && !glfwWindowShouldClose(mWindow)) {
            glfwPollEvents();

            int w, h;
            glfwGetWindowSize(mWindow, &w, &h);
            if (w != lastWidth || h != lastHeight) {
                lastWidth  = w;
                lastHeight = h;
                for (const auto& child : mChildren)
                    child->onWindowResize(nullptr, (size_t)w, (size_t)h);
            }

            ui::newFrame();

            sMainWindowDockID = ImGui::DockSpaceOverViewport();

            for (const auto& child : mChildren)
                child->onRender(nullptr);

            ui::renderFrame(mWindow);
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
};

Application::Application(size_t width, size_t height, float dpi)
    : mInternal(std::make_unique<ApplicationInternal>(width, height, dpi))
{
}

Application::~Application()
{
}

void Application::setTitle(const std::string& str)
{
    mInternal->setTitle(str.c_str());
}

bool Application::exec()
{
    return mInternal->exec();
}

void Application::addChild(const std::shared_ptr<Widget>& widget)
{
    mInternal->addChild(widget);
}

void Application::setDropCallback(const std::function<void(const Path&)>& callback)
{
    mInternal->setDropCallback(callback);
}

void Application::signalQuit()
{
    mInternal->signalQuit();
}

int Application::getMainWindowDockID()
{
    return sMainWindowDockID;
}
} // namespace IG
