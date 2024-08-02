#include "Application.h"
#include "DefaultConfig.h"
#include "HelpAboutWidget.h"
#include "IO.h"
#include "InspectorOptions.h"
#include "KlemsDataModel.h"
#include "Logger.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MenuSeparator.h"
#include "PropertyWidget.h"
#include "Runtime.h"
#include "RuntimeInfo.h"
#include "ViewWidget.h"
#include "measured/KlemsLoader.h"

#include "UI.h"

#include "portable-file-dialogs.h"

using namespace IG;

static void openFileCallback(const Path& path);
static void openFileDialogCallback();

class Context {
    Application* mApplication = nullptr;
    std::shared_ptr<HelpAboutWidget> mHelpAboutWidget;
    std::shared_ptr<ViewWidget> mViewWidget;
    std::shared_ptr<PropertyWidget> mPropertyWidget;

    std::shared_ptr<IDataModel> mLoadedModel;

    std::shared_ptr<MenuItem> mViewMenuItem;
    std::shared_ptr<MenuItem> mPropertyMenuItem;

public:
    void setup(Application* app)
    {
        mApplication     = app;
        mHelpAboutWidget = std::make_shared<HelpAboutWidget>();
        mViewWidget      = std::make_shared<ViewWidget>();
        mPropertyWidget  = std::make_shared<PropertyWidget>();

        mViewWidget->connectProperties(mPropertyWidget.get());
        mPropertyWidget->connectView(mViewWidget.get());

        app->addChild(setupMainMenu());
        app->addChild(mViewWidget);
        app->addChild(mPropertyWidget);
        app->addChild(mHelpAboutWidget);

        app->setDropCallback(openFileCallback);
    }

    static inline Context& instance()
    {
        static Context sContext;
        return sContext;
    }

    inline Application* app() { return mApplication; }

    void load(const Path& path)
    {
        if (KlemsLoader::check(path)) {
            auto klems = std::make_shared<Klems>();
            if (KlemsLoader::load(path, *klems))
                mLoadedModel = std::make_shared<KlemsDataModel>(klems);
        } else {
            // TODO
        }

        mViewWidget->setModel(mLoadedModel);
        mPropertyWidget->setModel(mLoadedModel);

        if (mApplication)
            mApplication->setTitle("Ignis | " + std::filesystem::weakly_canonical(path).generic_string());
    }

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
        fileMenu->add(std::make_shared<MenuItem>("Quit", [&](MenuItem*) { mApplication->signalQuit(); }));

        mViewMenuItem     = std::make_shared<MenuItem>("View", [](MenuItem* item) { item->setSelected(!item->isSelected()); });
        mPropertyMenuItem = std::make_shared<MenuItem>("Properties", [](MenuItem* item) { item->setSelected(!item->isSelected()); });

        // TODO: Settings
        mViewMenuItem->setSelected(true);
        mPropertyMenuItem->setSelected(true);

        viewMenu->add(mViewMenuItem);
        viewMenu->add(mPropertyMenuItem);

#ifdef IG_OS_WINDOWS
        viewMenu->add(std::make_shared<MenuSeparator>());
        viewMenu->add(std::make_shared<MenuItem>("Terminal", [](MenuItem*) { IG_LOGGER.openConsole(); }));
#endif

        mViewWidget->connectMenuItem(mViewMenuItem.get());
        mPropertyWidget->connectMenuItem(mPropertyMenuItem.get());

        helpMenu->add(std::make_shared<MenuItem>("Website", [](MenuItem*) { SDL_OpenURL("https://github.com/PearCoding/Ignis"); }));
        helpMenu->add(std::make_shared<MenuSeparator>());
        helpMenu->add(std::make_shared<MenuItem>("About", [&](MenuItem*) { mHelpAboutWidget->show(); }));

        return mainMenu;
    }
};

static void openFileCallback(const Path& path)
{
    Context::instance().load(path);
}

static void openFileDialogCallback()
{
    auto f = pfd::open_file("Choose measured BSDFs", pfd::path::home(),
                            { "Klems/Tensor Files (.xml)", "*.xml",
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

    IG_LOGGER.closeExtraConsole();

    InspectorOptions args(argc, argv, "Ignis BSDF Inspector");
    if (args.ShouldExit)
        return EXIT_SUCCESS;

    // Ensure directory is available
    std::filesystem::create_directories(RuntimeInfo::configDirectory());
    const std::string ini_file = (RuntimeInfo::configDirectory() / "ui_bsdf_inspector.ini").generic_string();

    try {
        Application app(args.WindowWidth, args.WindowHeight, args.DPI.value_or(-1));
        Context::instance().setup(&app);

        if (!args.InputFile.empty())
            openFileCallback(args.InputFile);

        auto& io       = ImGui::GetIO();
        io.IniFilename = ini_file.c_str();
        if (!std::filesystem::exists(io.IniFilename))
            LoadDefaultConfig();

        const int exitcode = app.exec() ? EXIT_SUCCESS : EXIT_FAILURE;

        return exitcode;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}