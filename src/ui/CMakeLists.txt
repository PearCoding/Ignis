CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME implot
    GITHUB_REPOSITORY epezent/implot
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME imgui_markdown
    GITHUB_REPOSITORY PearCoding/imgui_markdown 
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

#################################
SET(SRC_FILES 
    imgui_impl.cpp
    RangeSlider.cpp
    RangeSlider.h
    Timeline.cpp
    Timeline.h
    UIGlue.cpp
    UIGlue.h)

add_library(ig_lib_ui STATIC ${SRC_FILES})
target_include_directories(ig_lib_ui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${implot_SOURCE_DIR} ${imgui_markdown_SOURCE_DIR})
target_include_directories(ig_lib_ui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
target_link_libraries(ig_lib_ui PUBLIC SDL2::SDL2)
add_lto(ig_lib_ui)
add_checks(ig_lib_ui)
add_linker_options(ig_lib_ui)