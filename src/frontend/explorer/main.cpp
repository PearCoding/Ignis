#include "ExplorerOptions.h"
#include "Logger.h"
#include "MainWindow.h"
#include "Menu.h"
#include "MenuItem.h"
#include "ParameterWidget.h"
#include "RenderWidget.h"

#include "imgui.h"

#include "portable-file-dialogs.h"

using namespace IG;
static RenderWidget* sRenderWidget = nullptr;
static MainWindow* sMainWindow     = nullptr;

static void openFileCallback(const Path& path)
{
    if (sRenderWidget)
        sRenderWidget->openFile(path);
}

static void openFileDialogCallback()
{
    auto f = pfd::open_file("Choose scene", pfd::path::home(),
                            { "Scene Files (.json)", "*.json",
                              "All Files", "*" },
                            pfd::opt::none);

    const auto files = f.result();
    if (files.empty()) // Canceled
        return;

    openFileCallback(files[0]);
}

static std::shared_ptr<Menu> setupMainMenu()
{
    auto mainMenu = std::make_shared<Menu>("");
    auto fileMenu = std::make_shared<Menu>("File");
    mainMenu->add(fileMenu);
    auto helpMenu = std::make_shared<Menu>("Help");
    mainMenu->add(helpMenu);

    auto openFile = std::make_shared<MenuItem>("Open", openFileDialogCallback);
    fileMenu->add(openFile);
    fileMenu->add(std::make_shared<MenuItem>("Quit", []() { sMainWindow->signalQuit(); }));

    auto openWebsite = std::make_shared<MenuItem>("Website", []() {});
    helpMenu->add(openWebsite);

    return mainMenu;
}

int main(int argc, char** argv)
{
    if (!pfd::settings::available()) {
        std::cerr << "Portable File Dialogs are not available on this platform." << std::endl;
        return EXIT_FAILURE;
    }

    ExplorerOptions args(argc, argv, "Ignis Scene Explorer");
    if (args.ShouldExit)
        return EXIT_SUCCESS;

    try {
        MainWindow window(args.WindowWidth, args.WindowHeight, args.DPI.value_or(-1));
        sMainWindow = &window;

        auto renderWidget = std::make_shared<RenderWidget>();
        sRenderWidget     = renderWidget.get();

        if (!args.InputFile.empty())
            renderWidget->openFile(args.InputFile);

        window.addChild(renderWidget);
        window.addChild(setupMainMenu());
        window.addChild(std::make_shared<ParameterWidget>(sRenderWidget));

        window.setDropCallback(openFileCallback);
        return window.exec() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}