CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

add_library(ig_lib_ui STATIC imgui_sdl.cpp imgui_sdl.h imgui_markdown.h)
target_link_libraries(ig_lib_ui PUBLIC ${SDL2_LIBRARY})
target_include_directories(ig_lib_ui PUBLIC ${SDL2_INCLUDE_DIR} ${imgui_SOURCE_DIR})
target_include_directories(ig_lib_ui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
