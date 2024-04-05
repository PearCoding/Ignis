CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.90.4
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

SET(SRC_FILES 
    main.cpp
    imgui_impl.cpp
    Inspector.cpp
    Inspector.h
    Pose.cpp
    Pose.h
    PropertyView.cpp
    PropertyView.h
    UI.cpp
    UI.h)

add_executable(igview ${SRC_FILES})
target_include_directories(igview SYSTEM PUBLIC ${SDL2_INCLUDE_DIR} ${imgui_SOURCE_DIR} ${implot_SOURCE_DIR} ${imgui_markdown_SOURCE_DIR})
target_link_libraries(igview PRIVATE ${SDL2_LIBRARY} ig_lib_common)
add_lto(igview)
add_checks(igview)
add_linker_options(igview)
install(TARGETS igview ${_IG_RUNTIME_SET} COMPONENT frontends)
