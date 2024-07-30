#pragma once

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

#include "imgui.h"
#include "imgui_markdown.h"
#include "implot.h"

namespace IG::ui {
void setup(SDL_Window* window, SDL_Renderer* renderer, bool useDocking, float dpi = -1);
void shutdown();
void notifyResize(SDL_Window* window, SDL_Renderer* renderer);
void newFrame();
void renderFrame(SDL_Renderer* renderer);

bool processSDLEvent(const SDL_Event& event);
void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_);
float getFontScale(SDL_Window* window, SDL_Renderer* renderer);
} // namespace IG::ui