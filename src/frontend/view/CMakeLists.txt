file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igview ${SRC})
target_link_libraries(igview PRIVATE ig_lib_ui ig_lib_common)
add_lto(igview)
add_checks(igview)
add_linker_options(igview)
install(TARGETS igview ${_IG_RUNTIME_SET} COMPONENT frontends)
