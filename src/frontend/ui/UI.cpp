#include "UI.h"
#include "Logger.h"
#include "RuntimeInfo.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace IG::ui {
static float sDPI = -1;

void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_)
{
    switch (markdownFormatInfo_.type) {
    default:
        ImGui::defaultMarkdownFormatCallback(markdownFormatInfo_, start_);
        break;
    case ImGui::MarkdownFormatType::EMPHASIS:
        if (start_)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.16f, 0.57f, 0.94f, 1));
        else
            ImGui::PopStyleColor();
        break;
    }
}

float getFontScale(GLFWwindow* window)
{
    if (sDPI > 0)
        return sDPI;

    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    if (xscale <= 0)
        xscale = 1.0f;

    sDPI = xscale;
    IG_LOG(L_DEBUG) << "Detected DPI=" << sDPI << std::endl;
    return sDPI;
}

static void setupStandardFont(GLFWwindow* window)
{
    const auto dataPath = RuntimeInfo::readonlyDataPath();
    Path fontFile;
    if (!dataPath.empty())
        fontFile = dataPath / "fonts" / "UbuntuMono-R.ttf";

    if (!std::filesystem::exists(fontFile)) {
        IG_LOG(L_WARNING) << "Could not load custom font. Trying to use system default." << std::endl;
#if defined(IG_OS_WINDOWS)
        fontFile = "C:\\Windows\\Fonts\\consola.ttf";
#elif defined(IG_OS_APPLE)
        fontFile = "/System/Library/Fonts/TODO.otf";
#else
        fontFile = "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf";
#endif
    }

    auto& io = ImGui::GetIO();

    const float font_scaling_factor = getFontScale(window);
    if (std::filesystem::exists(fontFile)) {
        constexpr int DefaultFontSize = 13;
        ImFontConfig config;
        config.SizePixels    = DefaultFontSize * font_scaling_factor;
        config.PixelSnapH    = true;
        config.GlyphOffset.y = 1.0f * std::floor(config.SizePixels / (float)DefaultFontSize); // Add +1 offset per 13 units

        io.Fonts->AddFontFromFileTTF(fontFile.generic_string().c_str(), config.SizePixels, &config);
    }
}

static void glfwErrorCallback(int error, const char* description)
{
    IG_LOG(L_ERROR) << "GLFW error " << error << ": " << description << std::endl;
}

bool setup(GLFWwindow*& window, int width, int height, const std::string& title, bool useDocking, float dpi)
{
    sDPI = dpi;

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        IG_LOG(L_FATAL) << "Cannot initialize GLFW" << std::endl;
        return false;
    }

#if defined(IG_OS_APPLE)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        IG_LOG(L_FATAL) << "Cannot create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwSetWindowSizeLimits(window, 64, 64, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(window);
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
        IG_LOG(L_FATAL) << "Failed to load OpenGL" << std::endl;
        return false;
    }
    IG_LOG(L_DEBUG) << "Using OpenGL " << glGetString(GL_VERSION) << std::endl;

    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifdef IMGUI_HAS_DOCK
    if (useDocking)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#else
    IG_UNUSED(useDocking);
#endif

    ImGuiStyle& style  = ImGui::GetStyle();
    style.GrabRounding = 3;
    style.TabRounding  = 3;

#ifndef IG_OS_WINDOWS
    // Windows handles scaling different than other systems
    const float scale = getFontScale(window);
    style.ScaleAllSizes(scale);
#endif

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    setupStandardFont(window);
    return true;
}

void shutdown(GLFWwindow* window)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (window)
        glfwDestroyWindow(window);
    glfwTerminate();
}

void openURL(const std::string& url)
{
#if defined(IG_OS_WINDOWS)
    std::system(("start \"\" \"" + url + "\"").c_str());
#elif defined(IG_OS_APPLE)
    std::system(("open \"" + url + "\"").c_str());
#else
    std::system(("xdg-open \"" + url + "\" &").c_str());
#endif
}

void newFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void renderFrame(GLFWwindow* window)
{
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}
} // namespace IG::ui
