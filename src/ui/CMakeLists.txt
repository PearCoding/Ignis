CPMFindPackage(
    NAME imgui
    GIT_REPOSITORY git@github.com:ocornut/imgui.git
    GIT_TAG master
    DOWNLOAD_ONLY
)

add_library(ig_lib_ui STATIC Camera.h imgui_sdl.cpp imgui_sdl.h)
target_link_libraries(ig_lib_ui PUBLIC ${SDL2_LIBRARIES})
target_include_directories(ig_lib_ui PUBLIC ${SDL2_INCLUDE_DIRS} ${imgui_SOURCE_DIR})
target_include_directories(ig_lib_ui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)