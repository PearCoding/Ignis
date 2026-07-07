#pragma once

#include "opengl/glad.h"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

#include "imgui.h"
#include "imgui_markdown.h"
#include "implot.h"

#include <string>

namespace IG::ui {
// Create a GLFW window with an OpenGL context and initialize ImGui/ImPlot on it.
bool setup(GLFWwindow*& window, int width, int height, const std::string& title, bool useDocking, float dpi = -1);
void shutdown(GLFWwindow* window);
void newFrame();
void renderFrame(GLFWwindow* window);

void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_);
float getFontScale(GLFWwindow* window);

// Open a URL in the user's default browser.
void openURL(const std::string& url);
} // namespace IG::ui
