#pragma once

#include "IG_Config.h"

#include <functional>

namespace IG {
class Widget;
class MainWindow {
public:
    MainWindow(size_t width, size_t height, float dpi);
    ~MainWindow();

    void setTitle(const std::string& str);

    bool exec();

    void addChild(const std::shared_ptr<Widget>& widget);

    void setDropCallback(const std::function<void(const Path&)>& callback);

    void signalQuit();

private:
    std::unique_ptr<class MainWindowInternal> mInternal;
};
} // namespace IG