CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
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
    main.cpp
    imgui_sdl.cpp
    imgui_sdl.h
    Inspector.cpp
    Inspector.h
    Pose.cpp
    Pose.h
    UI.cpp
    UI.h)

add_executable(igview ${SRC_FILES})
target_include_directories(igview PUBLIC ${SDL2_INCLUDE_DIR} ${imgui_SOURCE_DIR} ${imgui_markdown_SOURCE_DIR})
target_link_libraries(igview PRIVATE ${SDL2_LIBRARY} ig_lib_common)
add_dependencies(igview ignis_drivers)
add_lto(igview)
add_checks(igview)
install(TARGETS igview COMPONENT runtime)
