file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

# add_executable(igexplorer ${SRC})
# target_link_libraries(igexplorer PRIVATE ig_lib_imgui ig_lib_common)
# add_lto(igexplorer)
# add_checks(igexplorer)
# add_linker_options(igexplorer)
# install(TARGETS igexplorer ${_IG_RUNTIME_SET} COMPONENT frontends)
