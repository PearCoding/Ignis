#pragma once

#include "IG_Config.h"

namespace IG {
class Widget;
class MainWindow {
public:
    MainWindow(size_t width, size_t height);
    ~MainWindow();

    bool exec();

    void addChild(const std::shared_ptr<Widget>& widget);

private:
    std::unique_ptr<class MainWindowInternal> mInternal;
};
} // namespace IG