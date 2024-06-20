#include "Logger.h"
#include "MainWindow.h"
#include "Menu.h"
#include "MenuItem.h"

#include "portable-file-dialogs.h"

using namespace IG;

static void openFileCallback()
{
    auto f = pfd::open_file("Choose scene", pfd::path::home(),
                            { "Scene Files (.json)", "*.json",
                              "All Files", "*" },
                            pfd::opt::none);

    const auto files = f.result();
    if (files.empty()) // Canceled
        return;

    std::cout << "Selected file: " << files[0] << std::endl;
}

static std::shared_ptr<Menu> setupMainMenu()
{
    auto mainMenu = std::make_shared<Menu>("");
    auto fileMenu = std::make_shared<Menu>("File");
    mainMenu->add(fileMenu);
    auto helpMenu = std::make_shared<Menu>("Help");
    mainMenu->add(helpMenu);

    auto openFile = std::make_shared<MenuItem>("Open", openFileCallback);
    fileMenu->add(openFile);

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

    try {
        MainWindow window(800, 600);

        window.addChild(setupMainMenu());

        return window.exec() ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception& e) {
        IG_LOG(L_FATAL) << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}