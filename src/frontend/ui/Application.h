#pragma once

#include "IG_Config.h"

#include <functional>

struct SDL_Renderer;

namespace IG {
class Widget;
class Application {
public:
    Application(size_t width, size_t height, float dpi);
    ~Application();

    void setTitle(const std::string& str);

    bool exec();

    void addChild(const std::shared_ptr<Widget>& widget);

    void setDropCallback(const std::function<void(const Path&)>& callback);

    void signalQuit();

    static int getMainWindowDockID();
    static SDL_Renderer* getRenderer();

private:
    std::unique_ptr<class ApplicationInternal> mInternal;
};
} // namespace IG