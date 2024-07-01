#include "ExplorerOptions.h"
#include "HelpAboutWidget.h"
#include "HelpControlWidget.h"
#include "Logger.h"
#include "MainWindow.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSeparator.h"
#include "OverviewWidget.h"
#include "ParameterWidget.h"
#include "RenderWidget.h"

#include "UI.h"

#include "portable-file-dialogs.h"

using namespace IG;
static RenderWidget* sRenderWidget           = nullptr;
static MainWindow* sMainWindow               = nullptr;
static HelpAboutWidget* sHelpAboutWidget     = nullptr;
static HelpControlWidget* sHelpControlWidget = nullptr;

static void openFileCallback(const Path& path)
{
    if (sRenderWidget)
        sRenderWidget->openFile(path);

    if (sMainWindow)
        sMainWindow->setTitle("Ignis | " + std::filesystem::weakly_canonical(path).generic_string());
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

    fileMenu->add(std::make_shared<MenuItem>("Open", openFileDialogCallback));
    fileMenu->add(std::make_shared<MenuSeparator>());
    fileMenu->add(std::make_shared<MenuItem>("Quit", []() { sMainWindow->signalQuit(); }));

    helpMenu->add(std::make_shared<MenuItem>("Controls", []() { sHelpControlWidget->show(); }));
    helpMenu->add(std::make_shared<MenuSeparator>());
    helpMenu->add(std::make_shared<MenuItem>("Website", []() { SDL_OpenURL("https://github.com/PearCoding/Ignis"); }));
    helpMenu->add(std::make_shared<MenuSeparator>());
    helpMenu->add(std::make_shared<MenuItem>("About", []() { sHelpAboutWidget->show(); }));

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

        auto helpControlWidget = std::make_shared<HelpControlWidget>();
        sHelpControlWidget     = helpControlWidget.get();

        auto helpAboutWidget = std::make_shared<HelpAboutWidget>();
        sHelpAboutWidget     = helpAboutWidget.get();

        window.addChild(renderWidget);
        window.addChild(setupMainMenu());
        window.addChild(std::make_shared<ParameterWidget>(sRenderWidget));
        window.addChild(std::make_shared<OverviewWidget>(sRenderWidget));
        window.addChild(helpControlWidget);
        window.addChild(helpAboutWidget);

        window.setDropCallback(openFileCallback);

        if (!args.InputFile.empty())
            openFileCallback(args.InputFile);

        return window.exec() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}