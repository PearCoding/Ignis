CPMAddPackage(
    NAME portable_file_dialogs
    GITHUB_REPOSITORY samhocevar/portable-file-dialogs
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igexplorer ${SRC})
target_include_directories(igexplorer SYSTEM PUBLIC ${portable_file_dialogs_SOURCE_DIR})
target_link_libraries(igexplorer PRIVATE ig_lib_imgui ig_lib_common)
add_lto(igexplorer)
add_checks(igexplorer)
add_linker_options(igexplorer)
install(TARGETS igexplorer ${_IG_RUNTIME_SET} COMPONENT frontends)
