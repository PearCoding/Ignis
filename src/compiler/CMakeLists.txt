file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(igc ${SRC})
target_link_libraries(igc PRIVATE ig_lib_common)
add_lto(igc)
add_checks(igc)
add_linker_options(igc)
install(TARGETS igc ${_IG_RUNTIME_SET} COMPONENT runtime)
