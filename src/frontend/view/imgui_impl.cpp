#include <SDL.h>

// Include implementation of imgui
#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "backends/imgui_impl_sdl.cpp"
#include "backends/imgui_impl_sdlrenderer.cpp"
#else
#include "imgui_old_sdl.cpp"
#endif

// Include implementation of implot
#include "implot.cpp"
#include "implot_items.cpp"
