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
#include "RegistryWidget.h"
#include "RenderWidget.h"

#include "UI.h"

#include "portable-file-dialogs.h"

using namespace IG;

static void openFileCallback(const Path& path);
static void openFileDialogCallback();

class Context {
    MainWindow* mMainWindow = nullptr;
    std::shared_ptr<RenderWidget> mRenderWidget;
    std::shared_ptr<HelpAboutWidget> mHelpAboutWidget;
    std::shared_ptr<HelpControlWidget> mHelpControlWidget;
    std::shared_ptr<ParameterWidget> mParameterWidget;
    std::shared_ptr<MenuItem> mParameterMenuItem;
    std::shared_ptr<OverviewWidget> mOverviewWidget;
    std::shared_ptr<MenuItem> mOverviewMenuItem;
    std::shared_ptr<RegistryWidget> mRegistryWidget;
    std::shared_ptr<MenuItem> mRegistryMenuItem;

public:
    void setup(MainWindow* window)
    {
        mMainWindow        = window;
        mRenderWidget      = std::make_shared<RenderWidget>();
        mHelpAboutWidget   = std::make_shared<HelpAboutWidget>();
        mHelpControlWidget = std::make_shared<HelpControlWidget>();
        mParameterWidget   = std::make_shared<ParameterWidget>(mRenderWidget.get());
        mOverviewWidget    = std::make_shared<OverviewWidget>(mRenderWidget.get());
        mRegistryWidget    = std::make_shared<RegistryWidget>(mRenderWidget.get());

        window->addChild(mRenderWidget);
        window->addChild(setupMainMenu());
        window->addChild(mParameterWidget);
        window->addChild(mOverviewWidget);
        window->addChild(mRegistryWidget);
        window->addChild(mHelpControlWidget);
        window->addChild(mHelpAboutWidget);

        window->setDropCallback(openFileCallback);
    }

    static inline Context& instance()
    {
        static Context sContext;
        return sContext;
    }

    inline MainWindow* window() { return mMainWindow; }
    inline RenderWidget* renderWidget() { return mRenderWidget.get(); }

private:
    std::shared_ptr<Menu> setupMainMenu()
    {
        auto mainMenu = std::make_shared<Menu>("");
        auto fileMenu = std::make_shared<Menu>("File");
        mainMenu->add(fileMenu);
        auto viewMenu = std::make_shared<Menu>("View");
        mainMenu->add(viewMenu);
        auto helpMenu = std::make_shared<Menu>("Help");
        mainMenu->add(helpMenu);

        fileMenu->add(std::make_shared<MenuItem>("Open", [](MenuItem*) { openFileDialogCallback(); }));
        fileMenu->add(std::make_shared<MenuSeparator>());
        fileMenu->add(std::make_shared<MenuItem>("Quit", [&](MenuItem*) { mMainWindow->signalQuit(); }));

        mParameterMenuItem = std::make_shared<MenuItem>("Parameter", [](MenuItem* item) { item->setSelected(!item->isSelected()); });
        mOverviewMenuItem  = std::make_shared<MenuItem>("Overview", [](MenuItem* item) { item->setSelected(!item->isSelected()); });
        mRegistryMenuItem  = std::make_shared<MenuItem>("Registry", [](MenuItem* item) { item->setSelected(!item->isSelected()); });

        mParameterMenuItem->setSelected(true);
        mOverviewMenuItem->setSelected(true);
        mRegistryMenuItem->setSelected(true);

        viewMenu->add(mParameterMenuItem);
        viewMenu->add(mOverviewMenuItem);
        viewMenu->add(mRegistryMenuItem);

        mParameterWidget->connectMenuItem(mParameterMenuItem.get());
        mOverviewWidget->connectMenuItem(mOverviewMenuItem.get());
        mRegistryWidget->connectMenuItem(mRegistryMenuItem.get());

        helpMenu->add(std::make_shared<MenuItem>("Controls", [&](MenuItem*) { mHelpControlWidget->show(); }));
        helpMenu->add(std::make_shared<MenuSeparator>());
        helpMenu->add(std::make_shared<MenuItem>("Website", [](MenuItem*) { SDL_OpenURL("https://github.com/PearCoding/Ignis"); }));
        helpMenu->add(std::make_shared<MenuSeparator>());
        helpMenu->add(std::make_shared<MenuItem>("About", [&](MenuItem*) { mHelpAboutWidget->show(); }));

        return mainMenu;
    }
};

static void openFileCallback(const Path& path)
{
    if (Context::instance().renderWidget())
        Context::instance().renderWidget()->openFile(path);

    if (Context::instance().window())
        Context::instance().window()->setTitle("Ignis | " + std::filesystem::weakly_canonical(path).generic_string());
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
        Context::instance().setup(&window);

        if (!args.InputFile.empty())
            openFileCallback(args.InputFile);

        const int exitcode = window.exec() ? EXIT_SUCCESS : EXIT_FAILURE;
        Context::instance().renderWidget()->cleanup();

        return exitcode;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}