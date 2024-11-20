#include "Application.h"
#include "DefaultConfig.h"
#include "ExplorerOptions.h"
#include "HelpAboutWidget.h"
#include "HelpControlWidget.h"
#include "Logger.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSeparator.h"
#include "OverviewWidget.h"
#include "ParameterWidget.h"
#include "RegistryWidget.h"
#include "RenderWidget.h"
#include "Runtime.h"
#include "RuntimeInfo.h"

#include "UI.h"

#include "portable-file-dialogs.h"

using namespace IG;

static void openFileCallback(const Path& path);
static void openFileDialogCallback();
static void exportFileDialogCallback(Runtime* runtime);

class Context {
    Application* mApplication = nullptr;
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
    void setup(Application* app, const ExplorerOptions& opts)
    {
        mApplication       = app;
        mRenderWidget      = std::make_shared<RenderWidget>(opts);
        mHelpAboutWidget   = std::make_shared<HelpAboutWidget>();
        mHelpControlWidget = std::make_shared<HelpControlWidget>();
        mParameterWidget   = std::make_shared<ParameterWidget>(mRenderWidget.get());
        mOverviewWidget    = std::make_shared<OverviewWidget>(mRenderWidget.get());
        mRegistryWidget    = std::make_shared<RegistryWidget>(mRenderWidget.get());

        app->addChild(mRenderWidget);
        app->addChild(setupMainMenu());
        app->addChild(mParameterWidget);
        app->addChild(mOverviewWidget);
        app->addChild(mRegistryWidget);
        app->addChild(mHelpControlWidget);
        app->addChild(mHelpAboutWidget);

        app->setDropCallback(openFileCallback);
    }

    static inline Context& instance()
    {
        static Context sContext;
        return sContext;
    }

    inline Application* app() { return mApplication; }
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
        fileMenu->add(std::make_shared<MenuItem>("Export image", [&](MenuItem*) { exportFileDialogCallback(mRenderWidget->currentRuntime()); }));
        fileMenu->add(std::make_shared<MenuSeparator>());
        fileMenu->add(std::make_shared<MenuItem>("Quit", [&](MenuItem*) { mApplication->signalQuit(); }));

        mParameterMenuItem = std::make_shared<MenuItem>("Parameter", [](MenuItem* item) { item->setSelected(!item->isSelected()); });
        mOverviewMenuItem  = std::make_shared<MenuItem>("Overview", [](MenuItem* item) { item->setSelected(!item->isSelected()); });
        mRegistryMenuItem  = std::make_shared<MenuItem>("Registry", [](MenuItem* item) { item->setSelected(!item->isSelected()); });

        // TODO: Settings
        mParameterMenuItem->setSelected(true);
        mOverviewMenuItem->setSelected(true);
        mRegistryMenuItem->setSelected(false);

        viewMenu->add(mParameterMenuItem);
        viewMenu->add(mOverviewMenuItem);
        viewMenu->add(mRegistryMenuItem);

#ifdef IG_OS_WINDOWS
        viewMenu->add(std::make_shared<MenuSeparator>());
        viewMenu->add(std::make_shared<MenuItem>("Terminal", [](MenuItem*) { IG_LOGGER.openConsole(); }));
#endif

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

    if (Context::instance().app())
        Context::instance().app()->setTitle("Ignis | " + std::string((const char*)std::filesystem::weakly_canonical(path).u8string().data()));
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

static void exportFileDialogCallback(Runtime* runtime)
{
    if (!runtime)
        return;

    // TODO: Add support for HDR
    auto f = pfd::save_file("Export image", pfd::path::home(),
                            { "OpenEXR", "*.exr" },
                            pfd::opt::none);

    const auto path = f.result();
    if (path.empty()) // Canceled
        return;

    if (!runtime->saveFramebuffer(path))
        IG_LOG(L_ERROR) << "Failed to save EXR file '" << path << "'" << std::endl;
    else
        IG_LOG(L_INFO) << "Screenshot saved to '" << path << "'" << std::endl;
}

int main(int argc, char** argv)
{
    if (!pfd::settings::available()) {
        std::cerr << "Portable File Dialogs are not available on this platform." << std::endl;
        return EXIT_FAILURE;
    }

    IG_LOGGER.closeExtraConsole();

    ExplorerOptions args(argc, argv, "Ignis Scene Explorer");
    if (args.ShouldExit)
        return EXIT_SUCCESS;

    // Ensure directory is available
    std::filesystem::create_directories(RuntimeInfo::configDirectory());
    const std::string ini_file = (RuntimeInfo::configDirectory() / "ui_explorer.ini").generic_string();

    try {
        Application app(args.WindowWidth, args.WindowHeight, args.DPI.value_or(-1));
        Context::instance().setup(&app, args);

        if (!args.InputFile.empty())
            openFileCallback(args.InputFile);

        auto& io       = ImGui::GetIO();
        io.IniFilename = ini_file.c_str();
        if (!std::filesystem::exists(io.IniFilename))
            LoadDefaultConfig();

        const int exitcode = app.exec() ? EXIT_SUCCESS : EXIT_FAILURE;
        Context::instance().renderWidget()->cleanup();

        return exitcode;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}