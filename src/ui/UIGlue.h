#pragma once

#include <SDL.h>

#include "imgui.h"
#include "imgui_markdown.h"
#include "implot.h"

namespace IGGui {
void setup(SDL_Window* window, SDL_Renderer* renderer);
void shutdown();
void notifyResize(SDL_Window* window, SDL_Renderer* renderer);
void newFrame();
void renderFrame();

void processSDLEvent(const SDL_Event& event);
void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_);
float getFontScale(SDL_Window* window, SDL_Renderer* renderer);
} // namespace IGGui