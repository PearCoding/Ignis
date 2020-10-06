include(ExternalProject)

ExternalProject_Add(imgui
    GIT_REPOSITORY    git@github.com:ocornut/imgui.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
)
ExternalProject_Get_Property(imgui source_dir)

add_library(ig_lib_ui STATIC imgui_sdl.cpp imgui_sdl.h)
add_dependencies(ig_lib_ui imgui)
target_link_libraries(ig_lib_ui PUBLIC ${SDL2_LIBRARIES})
target_include_directories(ig_lib_ui PUBLIC ${SDL2_INCLUDE_DIRS} ${source_dir})