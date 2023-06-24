#pragma once

#include <SDL.h>

#include "imgui.h"
#include "imgui_markdown.h"
#include "implot.h"

#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#else
#define USE_OLD_SDL
// The following implementation is deprecated and only available for old SDL versions
#include "imgui_old_sdl.h"
#endif

namespace IGGui {
void processSDLEventForImGUI(const SDL_Event& event);
void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_);
}