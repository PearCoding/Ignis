CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.90.6
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME implot
    GITHUB_REPOSITORY epezent/implot
    GIT_TAG v0.16
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME imgui_markdown
    GITHUB_REPOSITORY PearCoding/imgui_markdown 
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

#################################

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_library(ig_lib_imgui STATIC ${SRC})
target_include_directories(ig_lib_imgui SYSTEM PUBLIC ${SDL2_INCLUDE_DIR} ${imgui_SOURCE_DIR} ${implot_SOURCE_DIR} ${imgui_markdown_SOURCE_DIR})
target_link_libraries(ig_lib_imgui PUBLIC ${SDL2_LIBRARY})
add_lto(ig_lib_imgui)
add_checks(ig_lib_imgui)
add_linker_options(ig_lib_imgui)
# install(TARGETS ig_lib_imgui ${_IG_RUNTIME_SET} COMPONENT frontends)
