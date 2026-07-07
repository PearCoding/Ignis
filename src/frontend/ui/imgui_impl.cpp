#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_KEYIO

// Include implementation of imgui
#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

// Order matters: imgui_impl_opengl3.cpp ships its own GL loader that must be
// parsed before imgui_impl_glfw.cpp pulls in the system GL headers.
// clang-format off
#include "backends/imgui_impl_opengl3.cpp"
#include "backends/imgui_impl_glfw.cpp"
// clang-format on

// Include implementation of implot
#include "implot.cpp"
#include "implot_items.cpp"

// Ensure the following are explicitly instantiated
namespace ImGui {
template float ScaleRatioFromValueT<float, float, float>(ImGuiDataType data_type, float v, float v_min, float v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_size);
template float RoundScalarWithFormatT<float>(const char* format, ImGuiDataType data_type, float v);
} // namespace ImGui
