CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.91.0-docking
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

CPMAddPackage(
    NAME portable_file_dialogs
    GITHUB_REPOSITORY samhocevar/portable-file-dialogs
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

#################################

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_library(ig_ui STATIC ${SRC})
target_include_directories(ig_ui SYSTEM PUBLIC ${SDL2_INCLUDE_DIR} ${imgui_SOURCE_DIR} ${implot_SOURCE_DIR} ${imgui_markdown_SOURCE_DIR} ${portable_file_dialogs_SOURCE_DIR})
target_include_directories(ig_ui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ig_ui PUBLIC ${SDL2_LIBRARY} ig_runtime)
add_custom_command(TARGET ig_ui POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/fonts/ ${CMAKE_BINARY_DIR}/data/fonts)
ig_add_extra_options(ig_ui)

if(WIN32)
    set(INSTALL_DEST data/fonts)
else()
    set(INSTALL_DEST share/${PROJECT_NAME}/fonts)
endif()

install(DIRECTORY fonts/ COMPONENT frontends DESTINATION ${INSTALL_DEST})